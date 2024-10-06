/* MIT License
 *
 * Copyright (c) 2024 Andrew D Smith
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

#ifndef SRC_CONNECTION_MANAGER_HPP_
#define SRC_CONNECTION_MANAGER_HPP_

#include "connection.hpp"
#include <unordered_set>

struct connection_manager {
  // manages connections so they can be cleanly stopped
  connection_manager(const connection_manager&) = delete;
  connection_manager& operator=(const connection_manager&) = delete;
  connection_manager() {}
  auto start(connection_ptr c) -> void {
    connections.insert(c);
    c->start();
  }
  auto stop(connection_ptr c) -> void {
    connections.erase(c);
    c->stop();
  }
  auto stop_all() -> void {
    for (auto &c: connections) c->stop();
    connections.clear();
  }
  std::unordered_set<connection_ptr> connections;
};

#endif  // SRC_CONNECTION_MANAGER_HPP_
