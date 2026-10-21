-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A constant of the root architecture worked out from the root's string
-- generic, "real'value(Lim_g)", handed to an instance's generic: what
-- elaborating the root gives (14.4.2). open-logic's olo_fix_limit_tb takes
-- its limits as strings.

entity t_vhdl_realvaluegen_m is
  generic (L : real; N : integer);
  port (o : out real; cnt : out integer);
end entity;
architecture a of t_vhdl_realvaluegen_m is
begin
  o <= L;
  cnt <= N;
end architecture;

entity t_vhdl_realvaluegen is
  generic (Lim_g : string := "-1.5"; Num_g : string := "42");
end entity;
architecture a of t_vhdl_realvaluegen is
  constant Lim_c : real := real'value(Lim_g);
  constant Num_c : integer := integer'value(Num_g) + 1;
  signal o : real;
  signal n : integer;
begin
  u : entity work.t_vhdl_realvaluegen_m generic map (L => Lim_c, N => Num_c) port map (o, n);
  process
  begin
    wait for 1 ns;
    assert o = -1.5 report "o " & real'image(o) severity failure;
    assert n = 43 report "n " & integer'image(n) severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
