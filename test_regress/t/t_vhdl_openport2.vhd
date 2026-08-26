-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A port whose subtype leaves an index range undefined takes it from the
-- actual (5.3.2.2 e). The rule is written of "an interface object of an array
-- type, or a subelement of an interface object for which the subelement type
-- is an array type", so it reaches a record element and an array's own
-- element as well as the port's top level.

package p is
  type rec_t is record
    a : bit_vector;
    b : bit;
    c : bit_vector;
  end record;
  type words_t is array (natural range <>) of bit_vector;
end package;

use work.p.all;

entity sub is
  port (r  : in rec_t;
        w  : in words_t;
        v  : in bit_vector;
        oa : out integer;
        oc : out integer;
        ow : out integer;
        oe : out integer;
        ov : out integer);
end entity;

architecture rtl of sub is
begin
  oa <= r.a'length;
  oc <= r.c'length;
  ow <= w'length;
  oe <= w(w'low)'length;
  ov <= v'length;
end architecture;

use work.p.all;
use std.env.all;

entity t_vhdl_openport2 is
end entity;

architecture tb of t_vhdl_openport2 is
  signal r : rec_t(a(3 downto 0), c(1 to 6));
  signal w : words_t(0 to 2)(7 downto 0);
  signal v : bit_vector(4 downto 0);
  signal oa, oc, ow, oe, ov : integer;
begin

  u : entity work.sub port map (r => r, w => w, v => v,
                                oa => oa, oc => oc, ow => ow, oe => oe, ov => ov);

  main : process is
  begin
    wait for 1 ns;
    assert oa = 4 report "oa=" & integer'image(oa) severity failure;
    assert oc = 6 report "oc=" & integer'image(oc) severity failure;
    assert ow = 3 report "ow=" & integer'image(ow) severity failure;
    assert oe = 8 report "oe=" & integer'image(oe) severity failure;
    assert ov = 5 report "ov=" & integer'image(ov) severity failure;
    report "*-* All Finished *-*";
    stop;
  end process;

end architecture;
