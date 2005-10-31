// $Id$

#include "Display.hh"
#include "VideoSystem.hh"
#include "ScreenShotSaver.hh"
#include "EventDistributor.hh"
#include "FinishFrameEvent.hh"
#include "FileOperations.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "CommandConsole.hh"
#include "InfoCommand.hh"
#include "CliComm.hh"
#include "Scheduler.hh"
#include "RealTime.hh"
#include "Timer.hh"
#include "RenderSettings.hh"
#include "BooleanSetting.hh"
#include "VideoSourceSetting.hh"
#include "MSXMotherBoard.hh"
#include "VideoSystemChangeListener.hh"
#include <algorithm>
#include <cassert>
#include <iostream>

// Needed for workaround for "black flashes" problem in SDLGL renderer:
#include "SDLGLVideoSystem.hh"
#include "components.hh"

using std::string;
using std::vector;

namespace openmsx {

Display::Display(MSXMotherBoard& motherboard_)
	: currentRenderer(RendererFactory::UNINITIALIZED)
	, alarm(motherboard_.getEventDistributor())
	, screenShotCmd(motherboard_.getCommandController(), *this)
	, fpsInfo(motherboard_.getCommandController(), *this)
	, motherboard(motherboard_)
	, renderSettings(motherboard.getRenderSettings())
	, switchInProgress(0)
{
	// TODO clean up
	motherboard.getCommandConsole().setDisplay(this);

	frameDurationSum = 0;
	for (unsigned i = 0; i < NUM_FRAME_DURATIONS; ++i) {
		frameDurations.addFront(20);
		frameDurationSum += 20;
	}
	prevTimeStamp = Timer::getTime();

	EventDistributor& eventDistributor = motherboard.getEventDistributor();
	eventDistributor.registerEventListener(OPENMSX_FINISH_FRAME_EVENT,
			*this, EventDistributor::DETACHED);
	eventDistributor.registerEventListener(OPENMSX_DELAYED_REPAINT_EVENT,
			*this, EventDistributor::DETACHED);

	renderSettings.getRenderer().addListener(this);
	renderSettings.getFullScreen().addListener(this);
	renderSettings.getScaler().addListener(this);
	renderSettings.getVideoSource().addListener(this);
}

Display::~Display()
{
	renderSettings.getRenderer().removeListener(this);
	renderSettings.getFullScreen().removeListener(this);
	renderSettings.getScaler().removeListener(this);
	renderSettings.getVideoSource().removeListener(this);

	EventDistributor& eventDistributor = motherboard.getEventDistributor();
	eventDistributor.unregisterEventListener(OPENMSX_DELAYED_REPAINT_EVENT,
			*this, EventDistributor::DETACHED);
	eventDistributor.unregisterEventListener(OPENMSX_FINISH_FRAME_EVENT,
			*this, EventDistributor::DETACHED);

	resetVideoSystem();

	alarm.cancel();
	motherboard.getCommandConsole().setDisplay(0);
	assert(listeners.empty());
}

void Display::createVideoSystem()
{
	assert(!videoSystem.get());
	assert(currentRenderer == RendererFactory::UNINITIALIZED);
	checkRendererSwitch();
}

VideoSystem& Display::getVideoSystem()
{
	assert(videoSystem.get());
	return *videoSystem;
}

void Display::resetVideoSystem()
{
	videoSystem.reset();
	for (Layers::const_iterator it = layers.begin();
	     it != layers.end(); ++it) {
		std::cerr << (*it)->getName() << std::endl;
	}
	assert(layers.empty());
}

void Display::attach(VideoSystemChangeListener& listener)
{
	assert(count(listeners.begin(), listeners.end(), &listener) == 0);
	listeners.push_back(&listener);
}

void Display::detach(VideoSystemChangeListener& listener)
{
	Listeners::iterator it =
		find(listeners.begin(), listeners.end(), &listener);
	assert(it != listeners.end());
	listeners.erase(it);
}

Display::Layers::iterator Display::baseLayer()
{
	// Note: It is possible to cache this, but since the number of layers is
	//       low at the moment, it's not really worth it.
	Layers::iterator it = layers.end();
	while (true) {
		if (it == layers.begin()) {
			// There should always be at least one opaque layer.
			// TODO: This is not true for DummyVideoSystem.
			//       Anyway, a missing layer will probably stand out visually,
			//       so do we really have to assert on it?
			//assert(false);
			return it;
		}
		--it;
		if ((*it)->coverage == Layer::COVER_FULL) return it;
	}
}

void Display::signalEvent(const Event& event)
{
	if (event.getType() == OPENMSX_FINISH_FRAME_EVENT) {
		const FinishFrameEvent& ffe = static_cast<const FinishFrameEvent&>(event);
		VideoSource eventSource = ffe.getSource();
		VideoSource visibleSource =
			renderSettings.getVideoSource().getValue();

		bool draw = visibleSource == eventSource;
		if (draw) {
			repaint();
		}

		motherboard.getRealTime().sync(
			motherboard.getScheduler().getCurrentTime(), draw);
	} else if (event.getType() == OPENMSX_DELAYED_REPAINT_EVENT) {
		repaint();
	} else {
		assert(false);
	}
}

void Display::update(const Setting* setting)
{
	if (setting == &renderSettings.getRenderer()) {
		checkRendererSwitch();
	} else if (setting == &renderSettings.getFullScreen()) {
		checkRendererSwitch();
	} else if (setting == &renderSettings.getScaler()) {
		checkRendererSwitch();
	} else if (setting == &renderSettings.getVideoSource()) {
		checkRendererSwitch();
	} else {
		assert(false);
	}
}

void Display::checkRendererSwitch()
{
	if (switchInProgress) return;
	++switchInProgress;

	// Tell renderer to sync with render settings.
	RendererFactory::RendererID newRenderer =
		renderSettings.getRenderer().getValue();
	if ((newRenderer != currentRenderer) ||
	    !getVideoSystem().checkSettings()) {
		currentRenderer = newRenderer;
		
		for (Listeners::const_iterator it = listeners.begin();
		     it != listeners.end(); ++it) {
			(*it)->preVideoSystemChange();
		}

		resetVideoSystem();
		videoSystem.reset(RendererFactory::createVideoSystem(motherboard));

		for (Listeners::const_iterator it = listeners.begin();
		     it != listeners.end(); ++it) {
			(*it)->postVideoSystemChange();
		}
	}

	--switchInProgress;
}

void Display::repaint()
{
	alarm.cancel(); // cancel delayed repaint

	assert(videoSystem.get());
	// TODO: Is this the proper way to react?
	//       Behind this abstraction is SDL_LockSurface,
	//       which is severely underdocumented:
	//       it is unknown whether a failure is transient or permanent.
	if (!videoSystem->prepare()) return;

	for (Layers::iterator it = baseLayer(); it != layers.end(); ++it) {
		if ((*it)->coverage != Layer::COVER_NONE) {
			//std::cout << "Painting layer " << (*it)->getName() << std::endl;
			(*it)->paint();
		}
	}

	videoSystem->flush();

	// update fps statistics
	unsigned long long now = Timer::getTime();
	unsigned long long duration = now - prevTimeStamp;
	prevTimeStamp = now;
	frameDurationSum += duration - frameDurations.removeBack();
	frameDurations.addFront(duration);
}

void Display::repaintDelayed(unsigned long long delta)
{
	if (alarm.pending()) {
		// already a pending repaint
		return;
	}

#ifdef COMPONENT_GL
	if (dynamic_cast<SDLGLVideoSystem*>(videoSystem.get())) {
		// Ugly workaround: on GL limit the frame rate of the delayed
		// repaints. Because of a limitation in the SDL GL environment
		// we cannot render to texture (or aux buffer) and indirectly
		// this causes black flashes when a delayed repaint comes at
		// the wrong moment.
		// By limiting the frame rate we hope the delayed repaint only
		// get triggered during pause, where it's ok (is it?)
		if (delta < 200000) delta = 200000; // at most 8fps
	}
#endif

	alarm.schedule(delta);
}

void Display::addLayer(Layer& layer)
{
	int z = layer.getZ();
	Layers::iterator it;
	for (it = layers.begin(); it != layers.end() && (*it)->getZ() < z; ++it)
		/* do nothing */;
	layer.display = this;
	layers.insert(it, &layer);
}

void Display::removeLayer(Layer& layer)
{
	Layers::iterator it = find(layers.begin(), layers.end(), &layer);
	assert(it != layers.end());
	layers.erase(it);
}

void Display::updateZ(Layer& layer, Layer::ZIndex /*z*/)
{
	// Remove at old Z-index...
	Layers::iterator it = std::find(layers.begin(), layers.end(), &layer);
	assert(it != layers.end());
	layers.erase(it);

	// ...and re-insert at new Z-index.
	addLayer(layer);
}


// RepaintAlarm inner class

Display::RepaintAlarm::RepaintAlarm(EventDistributor& eventDistributor_)
	: eventDistributor(eventDistributor_)
{
}

void Display::RepaintAlarm::alarm()
{
	// Note: runs is seperate thread, use event mechanism to repaint
	//       in main thread
	eventDistributor.distributeEvent(
		new SimpleEvent<OPENMSX_DELAYED_REPAINT_EVENT>());
}


// ScreenShotCmd inner class:

Display::ScreenShotCmd::ScreenShotCmd(CommandController& commandController,
                                      Display& display_)
	: SimpleCommand(commandController, "screenshot")
	, display(display_)
{
}

string Display::ScreenShotCmd::execute(const vector<string>& tokens)
{
	string filename;
	switch (tokens.size()) {
	case 1:
		filename = FileOperations::getNextNumberedFileName("screenshots", "openmsx", ".png");
		break;
	case 2:
		if (tokens[1] == "-prefix") {
			throw SyntaxError();
		} else {
			filename = tokens[1];
		}
		break;
	case 3:
		if (tokens[1] == "-prefix") {
			filename = FileOperations::getNextNumberedFileName("screenshots", tokens[2], ".png");
		} else {
			throw SyntaxError();
		}
		break;
	default:
		throw SyntaxError();
	}

	display.getVideoSystem().takeScreenShot(filename);
	getCommandController().getCliComm().printInfo("Screen saved to " + filename);
	return filename;
}

string Display::ScreenShotCmd::help(const vector<string>& /*tokens*/) const
{
	return
		"screenshot              Write screenshot to file \"openmsxNNNN.png\"\n"
		"screenshot <filename>   Write screenshot to indicated file\n"
		"screenshot -prefix foo  Write screenshot to file \"fooNNNN.png\"\n";
}

// FpsInfoTopic inner class:

Display::FpsInfoTopic::FpsInfoTopic(CommandController& commandController,
                                    Display& display_)
	: InfoTopic(commandController, "fps")
	, display(display_)
{
}

void Display::FpsInfoTopic::execute(const vector<TclObject*>& /*tokens*/,
                                    TclObject& result) const
{
	double fps = 1000000.0 * NUM_FRAME_DURATIONS / display.frameDurationSum;
	result.setDouble(fps);
}

string Display::FpsInfoTopic::help (const vector<string>& /*tokens*/) const
{
	return "Returns the current rendering speed in frames per second.";
}

} // namespace openmsx

