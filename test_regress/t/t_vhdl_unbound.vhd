-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A component with no entity of its name, and no binding indication in
-- reach, is unbound: the instance is not elaborated, and each actual
-- associated with one of its formals takes that formal's default value
-- (7.3.3, 12.4.3). A port with no default of its own has the one its
-- subtype gives, which is its leftmost value (6.4.2.3) -- not whatever the
-- actual happened to start at.
--
-- The configuration below binds it, and is deliberately not elaborated: a
-- configuration declaration applies to the hierarchy built through it (3.4).
-- nvc 1.22.1 and GHDL 7.0.0-dev both answer this way.

entity t_vhdl_unbound_sub is
  port (i : in integer; o : out integer);
end entity;

architecture rtl of t_vhdl_unbound_sub is
begin
  o <= i;
end architecture;

use std.env.all;

entity t_vhdl_unbound is
end entity;

architecture tb of t_vhdl_unbound is
  component virtualcomp is
    port (i : in integer; o : out integer; d : out integer);
  end component;
  signal src : integer := 7;
  signal res : integer := 5;
  signal dflt : integer := 5;
begin

  u1 : virtualcomp port map (i => src, o => res, d => dflt);

  main: process is
  begin
    wait for 1 ns;
    assert res = integer'left
      report "an unbound instance's actual is " & integer'image(res)
        & ", not the formal's default" severity failure;
    assert dflt = integer'left
      report "the second actual is " & integer'image(dflt) severity failure;
    assert src = 7 report "an input actual was disturbed" severity failure;

    report "*-* All Finished *-*" severity note;
    stop(0);
  end process;

end architecture;

configuration t_vhdl_unbound_cfg of t_vhdl_unbound is
  for tb
    for u1 : virtualcomp use entity work.t_vhdl_unbound_sub(rtl)
      port map (i => i, o => o);
    end for;
  end for;
end;
