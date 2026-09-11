-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An architecture continues its entity's region, and a process's labels are
-- declared in the process's region, so neither declares a name twice; the
-- alternatives of a generate have distinct labels. Two use clauses making
-- visible two declarations of one name make neither visible. A use clause
-- looks into a library or a package, and nothing follows its ".all". A type
-- is not the prefix of a selected name.
-- References: nvc 1.22.1, GHDL 7.0.0-dev, Questa 2025.2.

package t_vhdl_visrules_p1 is
  constant k : natural := 1;
end package;

package t_vhdl_visrules_p2 is
  constant k : natural := 2;
end package;

entity t_vhdl_visrules_e is
end entity;

architecture a of t_vhdl_visrules_e is
begin
end architecture;

entity t_vhdl_visrules_bad is
  generic (width : natural := 4);
  port (clk : in bit);
end entity;

use work.t_vhdl_visrules_p1.all;
use work.t_vhdl_visrules_p2.all;
architecture a of t_vhdl_visrules_bad is
  type rec is record
    f : natural;
  end record;
  signal s : bit;
  -- Legal
  constant k1 : natural := work.t_vhdl_visrules_p1.k;
  -- Illegal
  signal clk : bit;
  constant width : natural := 3;
  constant k2 : natural := k;
  constant k3 : natural := rec.f;
  use work.t_vhdl_visrules_e.all;
  use work.t_vhdl_visrules_p1.all.k;
begin
  process
    variable v : natural;
  begin
    -- Legal
    l1 : for i in 0 to 1 loop
    end loop;
    -- Illegal
    v : for i in 0 to 1 loop end loop;
    l1 : for i in 0 to 1 loop end loop;
    wait;
  end process;
  -- Illegal
  g : if alt1 : width = 4 generate else alt1 : generate end generate;
end architecture;
