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

#include "metadata_is_consistent.hpp"

#include "cpg_index.hpp"
#include "methylome.hpp"

[[nodiscard]] auto
metadata_is_consistent(const methylome &meth, const cpg_index &index) -> bool {
  const auto versions_match = (index.meta.version == meth.meta.version);
  const auto index_hashes_match =
    (index.meta.index_hash == meth.meta.index_hash);
  const auto assemblies_match = (index.meta.assembly == meth.meta.assembly);
  const auto n_cpgs_match = (index.meta.n_cpgs == meth.meta.n_cpgs);
  return versions_match && index_hashes_match && assemblies_match &&
         n_cpgs_match;
}
