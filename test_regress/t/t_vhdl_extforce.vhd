-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A force through an external name overrides the signal's drivers until a
-- release hands it back to them (10.5.2.1, 8.7). UVVM's AXI VVC test
-- forces each response signal of its harness in turn, checks the VVC
-- notices, and releases it: a release that kept the forced value left every
-- signal forced, and the next check counted the wrong alerts.
-- nvc 1.22.1 and Questa 25.1std pass this; GHDL 7.0.0-dev stops with "NULL
-- access dereferenced" at the first force through an external name.

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_extforce_dut is
  port (src : in std_logic_vector(1 downto 0); resp_out : out std_logic_vector(1 downto 0));
end entity;

architecture a of t_vhdl_extforce_dut is
begin
  resp_out <= src;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_extforce_th is
end entity;

architecture a of t_vhdl_extforce_th is
  signal src   : std_logic_vector(1 downto 0) := "01";
  signal bresp : std_logic_vector(1 downto 0);
  signal rresp : std_logic_vector(1 downto 0);
  signal bid   : std_logic_vector(7 downto 0);
  signal dout  : std_logic_vector(1 downto 0);
begin
  bresp <= src;
  rresp <= src;
  bid <= x"0" & src & src;
  i_dut : entity work.t_vhdl_extforce_dut port map (src => src, resp_out => dout);
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_extforce is
end entity;

architecture a of t_vhdl_extforce is
begin
  i_th : entity work.t_vhdl_extforce_th;

  process
    alias bresp is << signal i_th.bresp : std_logic_vector >>;
    alias rresp is << signal i_th.rresp : std_logic_vector >>;
    alias bid   is << signal i_th.bid   : std_logic_vector >>;
    alias dout  is << signal i_th.i_dut.resp_out : std_logic_vector >>;
    alias dsig  is << signal i_th.dout : std_logic_vector >>;
  begin
    wait for 1 ns;
    assert bresp = "01" and rresp = "01" and bid = x"05" and dout = "01" severity failure;
    -- One signal forced, the other of the same shape left alone
    bresp <= force not bresp;
    wait for 1 ns;
    assert bresp = "10" report "forced bresp " & to_string(bresp) severity failure;
    assert rresp = "01" report "rresp followed bresp " & to_string(rresp) severity failure;
    bresp <= release;
    wait for 1 ns;
    assert bresp = "01" report "released bresp " & to_string(bresp) severity failure;
    -- Part of a signal
    bid(7 downto 4) <= force x"f";
    wait for 1 ns;
    assert bid = x"f5" report "part forced bid " & to_hstring(bid) severity failure;
    bid <= release;
    wait for 1 ns;
    assert bid = x"05" report "released bid " & to_hstring(bid) severity failure;
    -- An output port of an instance further down
    dout <= force "11";
    wait for 1 ns;
    assert dout = "11" report "forced port " & to_string(dout) severity failure;
    dout <= release;
    wait for 1 ns;
    assert dout = "01" report "released port " & to_string(dout) severity failure;
    -- A signal an instance's output drives
    dsig <= force "10";
    wait for 1 ns;
    assert dsig = "10" report "forced instance-driven signal " & to_string(dsig) severity failure;
    dsig <= release;
    wait for 1 ns;
    assert dsig = "01" report "released instance-driven signal " & to_string(dsig)
      severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
