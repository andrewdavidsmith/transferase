/* MIT License
 *
 * Copyright (c) 2025 Andrew D Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <asio.hpp>
#include <csignal>

auto
add_signal_set(asio::signal_set &signals) -> void {
  signals.add(SIGINT);   // Interrupt from keyboard
  signals.add(SIGTERM);  // Termination request
  signals.add(SIGABRT);  // Process abort signal
  signals.add(SIGSEGV);  // Invalid memory reference
  signals.add(SIGFPE);   // Floating-point exception
  signals.add(SIGILL);   // Illegal instruction
#ifdef SIGQUIT
  signals.add(SIGQUIT);  // Quit from keyboard
#endif
#ifdef SIGHUP
  signals.add(SIGHUP);  // Hangup detected
#endif
#ifdef SIGBUS
  signals.add(SIGBUS);  // Bus error
#endif
#ifdef SIGALRM
  signals.add(SIGALRM);  // Alarm clock
#endif
#ifdef SIGXCPU
  signals.add(SIGXCPU);  // CPU time limit exceeded
#endif
#ifdef SIGXFSZ
  signals.add(SIGXFSZ);  // File size limit exceeded
#endif
#ifdef SIGPIPE
  signals.add(SIGPIPE);  // Broken pipe
#endif
}
