-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A generic may be of any type (6.5.6.1), including one the back end cannot
-- carry as a single folded value -- an array of records holding a REAL. Such
-- a generic travels as data rather than as a parameter, and the component's
-- own default travels with it where the instance says nothing.

use std.env.all;

package t_vhdl_recgeneric_pkg is
  type info is record
    field_1 : integer;
    field_2 : real;
  end record;
  type stuff is array (integer range 1 to 2) of info;
end package;

use work.t_vhdl_recgeneric_pkg.all;

entity t_vhdl_recgeneric_leaf is
  generic (gn : integer := 0;
           gb : stuff := ((1, 1.5), (2, 2.5)));
  port (o : out integer);
end entity;

architecture rtl of t_vhdl_recgeneric_leaf is
begin
  o <= gn + gb(1).field_1 + gb(2).field_1
       + integer(gb(1).field_2 * 10.0) + integer(gb(2).field_2 * 10.0);
end architecture;

use work.t_vhdl_recgeneric_pkg.all;
use std.env.all;

entity t_vhdl_recgeneric is
end entity;

architecture tb of t_vhdl_recgeneric is
  component leafc is
    generic (gn : integer := 0;
             gb : stuff := ((10, 0.5), (20, 1.5)));
    port (o : out integer);
  end component;
  for all : leafc use entity work.t_vhdl_recgeneric_leaf(rtl);
  signal given, defaulted : integer;
begin

  -- The instance says what it is, records and all
  u1 : leafc generic map (gn => 100, gb => ((3, 0.5), (4, 1.5))) port map (o => given);
  -- The instance says nothing, so the component's own default travels
  u2 : leafc generic map (gn => 100) port map (o => defaulted);

  main : process is
  begin
    wait for 1 ns;
    assert given = 100 + 3 + 4 + 5 + 15
      report "given is " & integer'image(given) severity failure;
    assert defaulted = 100 + 10 + 20 + 5 + 15
      report "defaulted is " & integer'image(defaulted) severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
