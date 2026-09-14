-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A signal with no initial value starts at the leftmost value of its
-- subtype (6.4.2.2), and a port is a signal.  For INTEGER that is not zero,
-- and the difference is not only what a read reports: a port that starts at
-- the value about to be assigned to it never records the change.

entity sub is
  port ( i_int  : in  integer;
         o_int  : out integer;
         o_nat  : out natural;
         o_real : out real;
         o_lvl  : out severity_level );
end entity;

architecture rtl of sub is
begin
  -- Read before anything in here has driven them
  chk : process is
  begin
    assert o_int = integer'left
      report "an INTEGER out port did not start at INTEGER'LEFT" severity failure;
    assert o_nat = 0
      report "a NATURAL out port did not start at 0, which is its own left"
      severity failure;
    assert o_real = real'left
      report "a REAL out port did not start at REAL'LEFT" severity failure;
    assert o_lvl = note
      report "an enumeration out port did not start at its leftmost literal"
      severity failure;
    wait;
  end process;

  o_int <= transport i_int after 10 ns;
end architecture;

-------------------------------------------------------------------------------

use std.env.all;

entity t_vhdl_portinit is
end entity;

architecture tb of t_vhdl_portinit is
  signal a : integer := 0;
  signal b : integer;
  signal n : natural;
  signal r : real;
  signal l : severity_level;
begin

  u : entity work.sub
    port map ( i_int => a, o_int => b, o_nat => n, o_real => r, o_lvl => l );

  main : process is
  begin
    -- The signal the port drives starts there too, and stays there until
    -- the port's own driver delivers
    wait for 1 ns;
    assert b = integer'left
      report "the signal an out port drives did not start at INTEGER'LEFT"
      severity failure;
    -- At 10 ns the port takes the value of "a", which is zero: a change
    -- only because it did not start at zero
    wait for 10 ns;
    assert b = 0 report "the port never delivered its value" severity failure;
    report "*-* All Finished *-*";
    finish;
  end process;

end architecture;
