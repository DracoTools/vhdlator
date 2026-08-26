-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A generic of the top entity may be given its value on the command line with
-- -G, and since a VHDL name is written without regard to case (15.4), any
-- spelling of the generic's name must reach it. Each generic below is set
-- from the command line with a different mixture of upper and lower case.

use std.env.all;

entity t_vhdl_genset is
  generic (WIDTH   : integer := 1;
           NAME    : string  := "unset";
           SCALE   : real    := 0.0;
           ENABLED : boolean := false);
end entity;

architecture tb of t_vhdl_genset is
  signal v : bit_vector(WIDTH - 1 downto 0) := (others => '1');
begin

  main : process is
  begin
    assert WIDTH = 12 report "WIDTH=" & integer'image(WIDTH) severity failure;
    assert v'length = 12 report "v'length" severity failure;
    assert NAME = "hello" report "NAME=" & NAME severity failure;
    assert SCALE > 2.4 and SCALE < 2.6 report "SCALE=" & real'image(SCALE) severity failure;
    assert ENABLED report "ENABLED" severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
