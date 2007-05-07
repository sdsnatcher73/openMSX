// $Id$

#ifndef RECORDEDCOMMAND_HH
#define RECORDEDCOMMAND_HH

#include "Command.hh"
#include "MSXEventListener.hh"
#include <memory>

namespace openmsx {

class MSXCommandController;
class MSXEventDistributor;
class Scheduler;
class EmuTime;

/** Commands that directly influence the MSX state should send and events
  * so that they can be recorded by the event recorder. This class helps to
  * implement that.
  */
class RecordedCommand : public Command, private MSXEventListener
{
public:
	/** This is like the execute() method of the Command class, it only
	  * has an extra time parameter.
	  * There are two variants of this method.  The one with tclObjects is
	  * the fastest, the one with strings is often more convenient to use.
	  * Subclasses must reimplement exactly one of these two.
	  */
	virtual void execute(
		const std::vector<TclObject*>& tokens, TclObject& result,
		const EmuTime& time);
	virtual std::string execute(
		const std::vector<std::string>& tokens, const EmuTime& time);

	/** It's possible that in some cases the command doesn't need to be
	  * recorded after all (e.g. a query subcommand). In that case you can
	  * override this method. Return false iff the command doesn't need
	  * to be recorded.
	  * Similar to the execute() method above there are two variants of
	  * this method. However in this case it's allowed to override none
	  * or just one of the two variants (but not both).
	  * The default implementation always returns true (will always
	  * record). If this default implementation is fine but speed is very
	  * important (e.g. the debug command) it is still recommenced to
	  * override the TclObject variant of this method (and just return
	  * true).
	  */
	virtual bool needRecord(const std::vector<TclObject*>& tokens) const;
	virtual bool needRecord(const std::vector<std::string>& tokens) const;

protected:
	RecordedCommand(MSXCommandController& msxCommandController,
	                MSXEventDistributor& msxEventDistributor,
	                Scheduler& scheduler,
	                const std::string& name);
	virtual ~RecordedCommand();

private:
	// Command
	virtual void execute(const std::vector<TclObject*>& tokens,
	                     TclObject& result);

	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         const EmuTime& time);

	MSXEventDistributor& msxEventDistributor;
	Scheduler& scheduler;
	std::auto_ptr<TclObject> dummyResultObject;
	TclObject* currentResultObject;
};

} // namespace openmsx

#endif
