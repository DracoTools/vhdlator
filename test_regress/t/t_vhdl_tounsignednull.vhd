-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- TO_UNSIGNED whose size is a generic an instance makes 0: a null vector,
-- given to a record element of no elements. It was a part select of no
-- bits, which the back end refuses. Shaped after open-logic's
-- olo_intf_i2c_master, "v.ClkDivCnt := to_unsigned(1, ClkDivBits_g)".

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
entity t_vhdl_tounsignednull_m is
  generic (N : natural := 0);
  port (clk : in std_logic; tick : out std_logic);
end entity;
architecture a of t_vhdl_tounsignednull_m is
  type two_r is record
    cnt : unsigned(N - 1 downto 0);
    lim : unsigned(N - 1 downto 0);
    t : std_logic;
  end record;
  signal r : two_r := (cnt => (others => '0'), lim => (others => '0'), t => '0');
begin
  process (all)
    variable v : two_r;
  begin
    v := r;
    v.t := '0';
    if r.cnt >= r.lim or N = 0 then
      v.cnt := to_unsigned(1, N);
      assert v.cnt'length = N severity failure;
      v.t := '1';
    else
      v.cnt := r.cnt + 1;
    end if;
    tick <= v.t;
  end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_tounsignednull is
end entity;
architecture a of t_vhdl_tounsignednull is
  signal t0, t4 : std_logic;
begin
  u0 : entity work.t_vhdl_tounsignednull_m port map (clk => '0', tick => t0);
  u4 : entity work.t_vhdl_tounsignednull_m generic map (N => 4) port map (clk => '0', tick => t4);
  process
  begin
    wait for 1 ns;
    assert t0 = '1' and t4 = '1' report "t0 " & std_logic'image(t0) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
