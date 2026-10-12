-- DESCRIPTION: Verilator: VHDL inout port of an array of vectors, 'Z' on one element by turns
--
-- Two entities drive one element of an array-of-vectors signal through
-- inout ports, each releasing it with "ZZ" while the other drives; the
-- other element is driven throughout. Where neither drives, it is "ZZ".
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
package t_vhdl_inoutzelem_pkg is
  type arr_t is array (1 downto 0) of std_logic_vector(1 downto 0);
end package;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_inoutzelem_pkg.all;
entity t_vhdl_inoutzelem_b is
  port (io_a : inout arr_t; en : in std_logic);
end entity;
architecture a of t_vhdl_inoutzelem_b is
begin
  io_a(1) <= "11" when en = '1' else "ZZ";
  io_a(0) <= "01";
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_inoutzelem_pkg.all;
entity t_vhdl_inoutzelem_c is
  port (io_a : inout arr_t; en : in std_logic);
end entity;
architecture a of t_vhdl_inoutzelem_c is
begin
  io_a(1) <= "10" when en = '1' else "ZZ";
  io_a(0) <= "ZZ";
end architecture;

library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_inoutzelem_pkg.all;
entity t_vhdl_inoutzelem is end entity;
architecture a of t_vhdl_inoutzelem is
  signal w : arr_t;
  signal en_b, en_c : std_logic := '0';
begin
  ub : entity work.t_vhdl_inoutzelem_b port map (io_a => w, en => en_b);
  uc : entity work.t_vhdl_inoutzelem_c port map (io_a => w, en => en_c);
  process
  begin
    en_b <= '1';
    wait for 1 ns;
    assert w(1) = "11" and w(0) = "01" report "1" severity failure;
    en_b <= '0';
    en_c <= '1';
    wait for 1 ns;
    assert w(1) = "10" and w(0) = "01" report "2" severity failure;
    en_c <= '0';
    wait for 1 ns;
    assert w(1) = "ZZ" report "3" severity failure;
    assert w(0) = "01" report "4" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
