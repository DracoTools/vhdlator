-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A port of an array of records, its index constrained by a generic and the
-- records' open elements left to take their bounds from the actual (5.3.2.2;
-- GHDL gna issue1420). A signal declared from one element's bounds is as long.

library ieee;
use ieee.std_logic_1164.all;

package t_vhdl_openrecport_pkg is
  type rec_t is record
    data : std_ulogic_vector;
    valid : std_ulogic;
    chnl : std_ulogic_vector;
  end record;
  type rec_arr_t is array (natural range <>) of rec_t;
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_openrecport_pkg.all;

entity t_vhdl_openrecport_sub is
  generic (N : positive);
  port (a : in rec_arr_t(N downto 1);
        dlen, clen : out natural);
end entity;

architecture behav of t_vhdl_openrecport_sub is
  signal cur : rec_t(data(a(a'low).data'range), chnl(a(a'low).chnl'range));
begin
  dlen <= cur.data'length;
  clen <= cur.chnl'length;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_openrecport_pkg.all;

entity t_vhdl_openrecport is
end entity;

architecture behav of t_vhdl_openrecport is
  signal s : rec_arr_t(3 downto 1)(data(8 downto 0), chnl(1 downto 0));
  signal dlen, clen : natural;
begin
  dut : entity work.t_vhdl_openrecport_sub generic map (N => 3)
    port map (a => s, dlen => dlen, clen => clen);

  process begin
    wait for 1 ns;
    assert dlen = 9 and clen = 2
      report integer'image(dlen) & " " & integer'image(clen) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
