/*
	BSD 2-Clause License

	Copyright (c) 2022, Sebastian Musialik
	All rights reserved.

	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice, this
	   list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice,
	   this list of conditions and the following disclaimer in the documentation
	   and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <iostream>
#include <cstring>
#include <vector>
#include <unistd.h>

#include <stdexcept>
#include <stdio.h>

std::string exec(std::string cmd) {

  std::string data;
  FILE * stream;
  const int max_buffer = 256;
  char buffer[max_buffer];
  cmd.append(" 2>&1");

  stream = popen(cmd.c_str(), "r");

  if (stream) {
    while (!feof(stream))
      if (fgets(buffer, max_buffer, stream) != NULL) data.append(buffer);
    pclose(stream);
  }
  return data;
}

void logger(std::string message)
{
	std::cout << message << std::endl;
	exec("echo " + message + " | logger -t sebek-preventsleep-daemon");
}

int main(int argc, char **argv)
{
	std::vector<std::string> args;
	for (int i = 0; i < argc; ++i) {
        args.push_back(argv[i]);
	}

	if (getuid()) {
		logger("You need root privileges. Bye bye.");
		return 0;
	}

	if (args.size() < 2) {
		logger("This program needs a name of VM domain as an argument. Bye bye.\nUsage: ./preventsleep <domain name> <optional: --sleep-instantly>\n--sleep-instantly option puts host machine to sleep as soon as guest is sleeping.");
		return 0;
	}

	bool instantSleep = false;
	if (args.size() == 3) {
		if (args[2] == "--sleep-instantly") {
			instantSleep = true;
		}
	}
	std::string vmStatus = exec("virsh domstate " + args[1]);
	if (vmStatus.find("błąd") != std::string::npos || vmStatus.find("error") != std::string::npos) { // polish  language support
		logger("Wrong VM domain name. Bye bye.");
		return 0;
	}

	std::string serviceStatus = exec("systemctl status libvirt-nosleep@" + args[1]);
	bool preventSleep = serviceStatus.find("active (running)") != std::string::npos;
	int seconds = 0; // startup
	while (true) {
		if (vmStatus.find("uruchomiona") != std::string::npos || vmStatus.find("running") != std::string::npos) { // polish  language support
			if (!preventSleep) {
				logger("Machine is running. Preventing host suspend...");
				exec("systemctl start libvirt-nosleep@" + args[1]);
				preventSleep = true;
			}
		} else if (vmStatus.find("pmsuspended") != std::string::npos) {
			if (preventSleep) {
				logger("Machine is suspended. Allowing host suspend...");
				exec("systemctl stop libvirt-nosleep@" + args[1]);
				preventSleep = false;
				if (instantSleep) {
					logger("Instant sleep enabled. Suspending the host machine...");
					exec("systemctl suspend");
				}
			}
		} else {
			logger("Machine is off or in an unsupported status. Trying for " + std::to_string(seconds++) + " time... ");
			logger("Status: " + vmStatus.substr(0, vmStatus.find_first_of("\n")));
			if (seconds > 5) {
				exec("systemctl stop libvirt-nosleep@" + args[1]);
				logger("Bye bye!");
				return 0;
			}
		}

		sleep(1);
		vmStatus = exec("virsh domstate " + args[1]);
	}
}