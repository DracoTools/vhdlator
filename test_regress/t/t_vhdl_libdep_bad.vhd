-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A selected name rooted at a library a library clause names reaches a
-- design unit in that library, whether or not any use clause mentions it
-- (12.3). The unit has to be analyzed first, so its absence is a missing
-- design unit and is reported as one -- not left until the declaration
-- that needed it is used for something.

library absent;

package t_vhdl_libdep_bad_pkg is
  alias element_type is absent.missing_pkg.token_type;
  constant c : absent.other_pkg.count_type := 0;
end package;
