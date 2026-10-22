-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A constant of an open array type given the joining of two open generics
-- and an element, "(INIT_G & STOP_C & HDLR_G)", is as long as they are
-- together (9.2.5), which only elaboration knows; handed on to an
-- instance's generic, it has to be a value elaboration settles. Shaped after
-- surf's MdioLinkIrqHandler, "MDIO_PROG_C := (PROG_INIT_G & PROG_HDLR_G)".

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_catopengen_pkg is
  type InstType is record
    lst : std_logic;
    reg : natural range 0 to 31;
  end record;
  type ProgArray is array (natural range <>) of InstType;
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_catopengen_pkg.all;

entity t_vhdl_catopengen_child is
  generic (PROG_G : ProgArray);
  port (n : out natural; s : out natural);
end entity;

architecture rtl of t_vhdl_catopengen_child is
begin
  process
    variable acc : natural := 0;
  begin
    for i in PROG_G'range loop
      acc := acc + PROG_G(i).reg;
    end loop;
    n <= PROG_G'length;
    s <= acc;
    wait;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_catopengen_pkg.all;

entity t_vhdl_catopengen_mid is
  generic (INIT_G : ProgArray; HDLR_G : ProgArray);
  port (n : out natural; s : out natural);
end entity;

architecture rtl of t_vhdl_catopengen_mid is
  constant STOP_C : InstType := (lst => '1', reg => 20);
  constant PROG_C : ProgArray := (INIT_G & STOP_C & HDLR_G);
begin
  u : entity work.t_vhdl_catopengen_child generic map (PROG_G => PROG_C) port map (n => n, s => s);
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_catopengen_pkg.all;

entity t_vhdl_catopengen is
end entity;

architecture rtl of t_vhdl_catopengen is
  constant A : ProgArray := ((lst => '0', reg => 1), (lst => '1', reg => 2));
  constant B : ProgArray := (0 => (lst => '1', reg => 7));
  signal n, s : natural;
begin
  u : entity work.t_vhdl_catopengen_mid generic map (INIT_G => A, HDLR_G => B) port map (n => n, s => s);
  process
  begin
    wait for 1 ns;
    report integer'image(n) & " " & integer'image(s);
    assert n = 4 and s = 30 severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
