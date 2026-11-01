/*
 ___________________________________________
|    _     ___                        _     |
|   | |   |__ \                      | |    |
|   | |__    ) |__ _  __ _  ___ _ __ | |_   |
|   | '_ \  / // _` |/ _` |/ _ \ '_ \| __|  |  HTTP/2 AGENT FOR MOCK TESTING
|   | | | |/ /| (_| | (_| |  __/ | | | |_   |  Version 0.0.z
|   |_| |_|____\__,_|\__, |\___|_| |_|\__|  |  https://github.com/testillano/h2agent
|                     __/ |                 |
|                    |___/                  |
|___________________________________________|

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2021 Eduardo Ramos

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace h2agent
{
namespace model
{

class Vault;

/**
 * Resolves the provision references '#{key}' / '#{key/<json pointer>}' ONCE, at
 * provision load: every string value of the document (provision or array of
 * provisions) is scanned and each reference is replaced by the vault value
 * (strings as they are, any other value as its compact json text, like @{}).
 * The first '/' starts the pointer into the stored value.
 *
 * A reference which does not exist (key or pointer) is kept literally, like an
 * unknown @{} variable, and reported in `warning`. Single pass: a resolved text
 * is not scanned again. Object keys are not scanned. References are never
 * dynamic: later vault changes need the provision to be loaded again.
 *
 * @param doc Provision document, resolved in place
 * @param vault Vault to resolve against (null: every reference is unresolved)
 * @param warning Filled with the unresolved references when false is returned
 *
 * @return False when any reference was left unresolved (the document is still valid)
 */
bool resolveProvisionReferences(nlohmann::json &doc, const Vault *vault, std::string &warning);

}
}
