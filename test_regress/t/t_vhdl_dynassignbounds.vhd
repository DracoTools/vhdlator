-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- Signals and variables whose bounds only the run settles -- a generate's
-- constants read from an array an impure function fills -- given values
-- that carry other bounds: an assignment keeps the target's index range and
-- matches the elements by position (10.6.2.1), so "s0 <= ('0' & aa) + bb"
-- leaves s0 "(HI + 1 downto LO)" and a slice of it by those ends reads the
-- right elements. s0 took the sum's "5 downto 0". Shaped after PoC's
-- arith_addw.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_dynassignbounds_m is
  generic (N : positive := 9; K : positive := 2);
  port (a, b : in std_logic_vector(N - 1 downto 0); s : out std_logic_vector(N - 1 downto 0));
end entity;
architecture rtl of t_vhdl_dynassignbounds_m is
  type integer_vector is array (natural range <>) of integer;
  impure function compute_blocks return integer_vector is
    variable res : integer_vector(K - 1 downto 0);
  begin
    for i in res'range loop
      res(i) := ((i + 1) * (N - K * (K - 1) / 2) + K / 2) / K + (i + 1) * i / 2;
    end loop;
    return res;
  end compute_blocks;
  constant BLOCKS : integer_vector(K - 1 downto 0) := compute_blocks;
begin
  blk : block
    constant M : positive := BLOCKS(0);
  begin
    s(M - 1 downto 0) <= std_logic_vector(unsigned(a(M - 1 downto 0)) + unsigned(b(M - 1 downto 0)));
  end block;
  gen : for i in 1 to K - 1 generate
    constant LO : positive := BLOCKS(i - 1);
    constant HI : positive := BLOCKS(i) - 1;
    signal aa, bb, ss : unsigned(HI downto LO);
    signal s0 : unsigned(HI + 1 downto LO);
  begin
    aa <= unsigned(a(HI downto LO));
    bb <= unsigned(b(HI downto LO));
    s0 <= ('0' & aa) + bb;
    ss <= s0(HI downto LO);
    s(HI downto LO) <= std_logic_vector(ss);
    process
      variable v : unsigned(HI + 1 downto LO);
    begin
      wait for 1 ns;
      assert s0'left = HI + 1 and s0'right = LO report "s0 " & integer'image(s0'left)
        severity failure;
      v := ('0' & aa) + bb;
      assert v'left = HI + 1 and v = s0 report "v " & integer'image(v'left) severity failure;
      wait;
    end process;
  end generate;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_dynassignbounds is
end entity;
architecture a of t_vhdl_dynassignbounds is
  signal s : std_logic_vector(8 downto 0);
begin
  u : entity work.t_vhdl_dynassignbounds_m port map (a => "011000011", b => "001000101", s => s);
  process
  begin
    wait for 2 ns;
    assert s = "100001000" report "s " & to_string(s) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
