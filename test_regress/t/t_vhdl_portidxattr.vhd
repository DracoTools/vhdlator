-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- An element of a signal named by an attribute of the signal's own subtype
-- is a static name, and may be a port's actual: "a_rsp_o => core_rsp(
-- core_rsp'low)", as neorv32's top level writes it (6.5.6.3, 9.4.3)

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_portidxattr_sub is
  port (o : out std_logic_vector(3 downto 0));
end entity;
architecture a of t_vhdl_portidxattr_sub is
begin
  o <= "1010";
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_portidxattr is
end entity;

architecture a of t_vhdl_portidxattr is
  type rsp_t is array (1 to 2) of std_logic_vector(3 downto 0);
  signal rsp : rsp_t := (others => (others => '0'));
begin
  u_lo : entity work.t_vhdl_portidxattr_sub port map (o => rsp(rsp'low));
  u_hi : entity work.t_vhdl_portidxattr_sub port map (o => rsp(rsp'high));
  process
  begin
    wait for 1 ns;
    assert rsp(1) = "1010" and rsp(2) = "1010" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
