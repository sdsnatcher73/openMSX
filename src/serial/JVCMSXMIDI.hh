#ifndef JVCMSXMIDI_HH
#define JVCMSXMIDI_HH

#include "MSXDevice.hh"
#include "MC6850.hh"
#include "serialize_meta.hh"

namespace openmsx {

class JVCMSXMIDI final : public MSXDevice
{
public:
	explicit JVCMSXMIDI(const DeviceConfig& config);

	// MSXDevice
	void reset(EmuTime::param time) override;
	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	MC6850 mc6850;
};
SERIALIZE_CLASS_VERSION(JVCMSXMIDI, 1);

} // namespace openmsx

#endif
