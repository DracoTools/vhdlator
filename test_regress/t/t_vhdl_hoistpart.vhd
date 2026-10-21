-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An out port whose width an impure function of a generic record gives,
-- the record made by an impure function (a protected counter numbers it),
-- associated an element at a time: "tdata(0) => n_l, tdata(1) => r_l". Each
-- actual follows its own element (6.5.7.1). VUnit's axi_stream_master in
-- open-logic's packet FIFO testbench is connected that way, its tdata as
-- long as data_length(master).

package t_vhdl_hoistpart_pkg is
  type rec_t is record
    p_len : natural;
    h : integer;
  end record;
  impure function new_rec(len : natural) return rec_t;
  impure function data_len(r : rec_t) return natural;
end;
package body t_vhdl_hoistpart_pkg is
  type counter_t is protected
    impure function next_id return integer;
  end protected;
  type counter_t is protected body
    variable n : integer := 0;
    impure function next_id return integer is
    begin
      n := n + 1;
      return n;
    end function;
  end protected body;
  shared variable counter : counter_t;
  impure function new_rec(len : natural) return rec_t is
  begin
    return (p_len => len, h => counter.next_id);
  end function;
  impure function data_len(r : rec_t) return natural is
  begin
    return r.p_len;
  end function;
end;
library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_hoistpart_pkg.all;
entity t_vhdl_hoistpart_vc is
  generic (m : rec_t);
  port (clk : in std_logic; tdata : out std_logic_vector(data_len(m) - 1 downto 0) := (others => '0'));
end;
architecture a of t_vhdl_hoistpart_vc is
begin
  process begin
    wait until rising_edge(clk);
    tdata <= "10";
    wait until rising_edge(clk);
    tdata <= "01";
    wait;
  end process;
end;
library ieee;
use ieee.std_logic_1164.all;
use work.t_vhdl_hoistpart_pkg.all;
entity t_vhdl_hoistpart is
end entity;
architecture a of t_vhdl_hoistpart is
  constant M : rec_t := new_rec(2);
  signal clk : std_logic := '0';
  signal n_l, r_l : std_logic;
begin
  clk <= not clk after 5 ns;
  u : entity work.t_vhdl_hoistpart_vc generic map (m => M) port map (clk => clk, tdata(0) => n_l, tdata(1) => r_l);
  process
  begin
    wait until rising_edge(clk);
    wait for 1 ns;
    assert n_l = '0' and r_l = '1'
      report "first n=" & std_logic'image(n_l) & " r=" & std_logic'image(r_l) severity failure;
    wait until rising_edge(clk);
    wait for 1 ns;
    assert n_l = '1' and r_l = '0'
      report "second n=" & std_logic'image(n_l) & " r=" & std_logic'image(r_l) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end;
