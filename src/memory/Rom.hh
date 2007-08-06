// $Id$

#ifndef ROM_HH
#define ROM_HH

#include "openmsx.hh"
#include "noncopyable.hh"
#include <string>
#include <memory>
#include <cassert>

namespace openmsx {

class MSXMotherBoard;
class EmuTime;
class XMLElement;
class File;
class RomInfo;
class GlobalCliComm;
class RomDebuggable;

class Rom : private noncopyable
{
public:
	Rom(MSXMotherBoard& motherBoard, const std::string& name,
	    const std::string& description, const XMLElement& config);
	Rom(MSXMotherBoard& motherBoard, const std::string& name,
	    const std::string& description, const XMLElement& config,
	    const std::string& id);
	virtual ~Rom();

	const byte& operator[](unsigned address) const {
		assert(address < size);
		return rom[address];
	}
	unsigned getSize() const { return size; }

	const RomInfo& getInfo() const;
	const std::string& getName() const;
	const std::string& getDescription() const;
	const std::string& getSHA1Sum() const;

private:
	void init(GlobalCliComm& cliComm, const XMLElement& config);
	void read(const XMLElement& config);
	bool checkSHA1(const XMLElement& config);

	MSXMotherBoard& motherBoard;
	const byte* rom;
	byte* extendedRom;

	std::auto_ptr<File> file;
	std::auto_ptr<RomInfo> info;
	std::auto_ptr<RomDebuggable> romDebuggable;

	mutable std::string sha1sum;
	std::string name;
	const std::string description;
	unsigned size;
};

} // namespace openmsx

#endif
