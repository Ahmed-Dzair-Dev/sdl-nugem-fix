/*
 * Copyright (c) 2016 Victor Nivet
 *
 * This file is part of Nugem.
 *
 * Nugem is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 * Nugem is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 *  along with Nugem.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cmd.hpp"
#include "mugenutils.hpp"
#include <fstream>
#include <string>
#include <memory>


namespace Nugem {
namespace Mugen {
	
void CharacterCommands::readFile(const std::string & filepath)
{
	MugenTextFile cmdfile(filepath);
	MugenTextKeyValue kv;
	std::unique_ptr<CommandDefinition> currentDefinition;
	std::unique_ptr<StateEntry> currentStateEntry;
	while ((kv = cmdfile.nextValue())) {
		if (cmdfile.newSection()) {
			if (currentDefinition)
				m_commands.push_back(std::unique_ptr<CommandDefinition>(currentDefinition.release()));
			if (cmdfile.section() == "Command")
				currentDefinition.reset(new CommandDefinition());
		}
		if (currentDefinition) {
			if (kv.name() == "name")
				currentDefinition->name = kv.value();
			else if (kv.name() == "command")
				currentDefinition->inputs = readInputDefinition(kv.value());
			else if (kv.name() == "time")
				currentDefinition->time = std::stoi(kv.value());
			else if (kv.name() == "buffer.time")
				currentDefinition->buffertime = std::stoi(kv.value());
		}
	}
}

std::vector<std::unique_ptr<CharacterCommands::CommandInput>> CharacterCommands::readInputDefinition(const std::string& entryString)
{
	std::vector<std::unique_ptr<CharacterCommands::CommandInput>> result;
	for (size_t index = 0; index < entryString.size(); index++)
	{
		char ch = entryString[index];
		bool isDirection = false;
		std::vector<CharacterCommands::CommandButtonModif> buttonList;
		CommandButtonModif currentButton;
		bool wideDirection = false; // detect the direction as 4-way
		bool exclusive = false;
		std::string directionString;
		// Between commas: block for a single symbol
		while (ch != ',' && index < entryString.size())
		{
			if (ch >= 'a' && ch <= 'z')
			{
				isDirection = false;
				switch (ch) {
					case 'a':
						currentButton.button = a;
						break;
					case 'b':
						currentButton.button = b;
						break;
					case 'c':
						currentButton.button = c;
						break;
					case 'x':
						currentButton.button = x;
						break;
					case 'y':
						currentButton.button = y;
						break;
					case 'z':
						currentButton.button = z;
						break;
				};
			}
			else
			if (ch >= 'A' && ch <= 'Z')
			{
				directionString += ch;
				isDirection = true;
			}
			else if (ch >= '0' && ch <= '9')
				currentButton.chargeTicks = currentButton.chargeTicks * 10 + (ch - '0');
			else if (ch == '$')
				wideDirection = true;
			else if (ch == '~')
				currentButton.released = true;
			else if (ch == '/')
				currentButton.heldDown = true;
			else if (ch == '+')
			{
				buttonList.push_back(currentButton);
				currentButton.heldDown = false;
				currentButton.released = false;
				currentButton.chargeTicks = 0;
				isDirection = false;
			}
			else if (ch == '<')
				exclusive = true;
			index++;
			ch = entryString[index];
		}
		if (isDirection)
		{
			CommandInputDirection *direction = nullptr;
			if (directionString == "B")
				direction = new CommandInputDirection(B);
			else if (directionString == "DB")
				direction = new CommandInputDirection(DB);
			else if (directionString == "UB")
				direction = new CommandInputDirection(UB);
			else if (directionString == "U")
				direction = new CommandInputDirection(U);
			else if (directionString == "UF")
				direction = new CommandInputDirection(UF);
			if (directionString == "F")
				direction = new CommandInputDirection(F);
			else if (directionString == "DF")
				direction = new CommandInputDirection(DF);
			else if (directionString == "D")
				direction = new CommandInputDirection(D);
			if (direction)
			{
				direction->wideDirection = wideDirection;
				direction->exclusive = exclusive;
				direction->chargeTicks = currentButton.chargeTicks;
				direction->heldDown = currentButton.heldDown;
				direction->released = currentButton.released;
				result.push_back(std::unique_ptr<CommandInput>(direction));
			}
		}
		else
		{
			buttonList.push_back(currentButton);
			CommandInputButtons *buttons = new CommandInputButtons;
			buttons->symbols = buttonList;
			buttons->exclusive = exclusive;
			result.push_back(std::unique_ptr<CommandInput>(buttons));
		}
	}
	return result;
}

}
}
