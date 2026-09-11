-- DESCRIPTION: Verilator: Verilog Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A generic is of mode in. "others" is the one choice of the last
-- alternative. A null transaction turns off a driver, which only a guarded
-- signal has. An entity's statements are passive. A deferred constant's full
-- declaration repeats its subtype as the deferred one writes it.

package t_vhdl_structrules_pkg is
  subtype small is integer range 0 to 7;
  constant ok_deferred : small;
  constant bad_deferred : small;
end package;

package body t_vhdl_structrules_pkg is
  constant ok_deferred : small := 1;
  constant bad_deferred : integer := 2;
end package body;

entity t_vhdl_structrules_bad is
  generic (g_ok : in integer := 1; g_bad : out integer := 2);
  port (p : in bit; q : out bit);
begin
  -- Legal: a passive process only watches
  assert p = '0' or p = '1';
  watch : process (p)
    variable seen : natural := 0;
  begin
    seen := seen + 1;
  end process;
  -- Illegal
  drive : process
  begin
    q <= '1';
    wait;
  end process;
end entity;

architecture a of t_vhdl_structrules_bad is
  signal plain : bit;
  signal guarded_bus : bit bus;
  signal sel : integer;
begin
  process
  begin
    guarded_bus <= null;
    plain <= null;
    case sel is
      when 0 => null;
      when others | 1 => null;
    end case;
    case sel is
      when others => null;
      when 2 => null;
    end case;
    wait;
  end process;
end architecture;
