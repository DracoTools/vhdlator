-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A design unit declared twice, two units that depend on each other, and a
-- unit that depends on one no library holds.

package dup is
end package;

package dup is
end package;

use work.circ_b.all;
package circ_a is
end package;

use work.circ_a.all;
package circ_b is
end package;

use work.nowhere.all;
entity t is
end entity;
