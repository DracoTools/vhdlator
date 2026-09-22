-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A record holding a REAL has no packed form, so a generic of that type is
-- handed over at elaboration rather than carried as a parameter. Which of
-- the two it is has to be decided the same way however the association is
-- written: by position the association names no formal, and the generic it
-- reaches is the one the unit declares at that place.
--
-- Written through a component and a binding indication, so that all three
-- association lists -- the instance's, the binding's, and the entity's own
-- order -- are exercised positionally.

package gendatapkg is
  type rectype is record
    ch  : character;
    int : integer;
    re  : real;
  end record;
  function mkrec (n : integer) return rectype;
end package;

package body gendatapkg is
  function mkrec (n : integer) return rectype is
  begin
    return (ch => 'a', int => n, re => 0.25);
  end function;
end package body;

use work.gendatapkg.all;

entity t_vhdl_gendata_sub is
  generic (constant gc : rectype;
           constant gn : integer);
end entity;

architecture rtl of t_vhdl_gendata_sub is
begin
  ck : process
  begin
    assert gc.int = 1234 report "bad integer element" severity failure;
    assert gc.ch = 'a' report "bad character element" severity failure;
    assert gc.re = 0.25 report "bad real element" severity failure;
    assert gn = 9 report "bad scalar generic beside it" severity failure;
    report "*-* All Finished *-*";
    std.env.stop;
    wait;
  end process;
end architecture;

use work.gendatapkg.all;

entity t_vhdl_gendata is
end entity;

architecture rtl of t_vhdl_gendata is
  component comp
    generic (constant dgc : rectype;
             constant dgn : integer);
  end component;
  -- The binding associates the entity's generics, positionally
  for u1 : comp use entity work.t_vhdl_gendata_sub(rtl) generic map (dgc, dgn);
begin
  -- The instance associates the component's generics, positionally
  u1 : comp generic map (mkrec(1234), 9);
end architecture;
