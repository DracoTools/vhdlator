-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A port or generic whose subtype is left unconstrained takes the subtype of
-- the actual it is given (5.3.2.2, 6.5.6.3). The bounds are not the same at
-- every instance, so each one is elaborated with the bounds it was handed --
-- including which way round its range runs.

entity t_vhdl_open_sub is
  generic (tag : bit_vector := "11");
  port (i : in bit_vector;
        o : out bit_vector;
        len : out integer;
        lft : out integer;
        rgt : out integer;
        hi : out integer;
        lo : out integer;
        asc : out boolean;
        tlen : out integer);
end entity;

architecture rtl of t_vhdl_open_sub is
begin
  len <= i'length;
  lft <= i'left;
  rgt <= i'right;
  hi <= i'high;
  lo <= i'low;
  asc <= i'ascending;
  tlen <= tag'length;

  -- Walking the port's own range, whichever way it runs, and writing the
  -- result into the output's range the same way round
  process (i) is
    variable k : integer;
  begin
    k := o'left;
    for n in i'range loop
      o(k) <= i(n);
      if o'ascending then
        k := k + 1;
      else
        k := k - 1;
      end if;
    end loop;
  end process;
end architecture;

use std.env.all;

entity t_vhdl_openport is
end entity;

architecture tb of t_vhdl_openport is
  signal a : bit_vector(7 downto 0) := "00001111";
  signal b : bit_vector(7 downto 0);
  signal c : bit_vector(0 to 3) := "1100";
  signal d : bit_vector(0 to 3);
  signal alen, alft, argt, ahi, alo, atlen : integer;
  signal clen, clft, crgt, chi, clo, ctlen : integer;
  signal aasc, casc : boolean;
  signal e : bit_vector(3 downto 0);
begin

  -- One entity, two instances, different widths and opposite directions
  u1 : entity work.t_vhdl_open_sub
    port map (i => a, o => b, len => alen, lft => alft, rgt => argt, hi => ahi,
              lo => alo, asc => aasc, tlen => atlen);

  u2 : entity work.t_vhdl_open_sub
    generic map (tag => "1010")
    port map (i => c, o => d, len => clen, lft => clft, rgt => crgt, hi => chi,
              lo => clo, asc => casc, tlen => ctlen);

  -- A block is elaborated where it stands, so its port takes the one subtype
  -- it is ever given
  blk : block is
    port (p : in bit_vector; q : out bit_vector);
    port map (a, e);
  begin
    assert p'length = 8 report "block port length" severity failure;
    assert p'left = 7 report "block port left" severity failure;
    assert q'length = 4 report "block output port length" severity failure;
    q <= p(3 downto 0);
  end block;

  process is
  begin
    wait for 1 ns;

    assert alen = 8 and clen = 4 report "'length" severity failure;
    assert alft = 7 and clft = 0 report "'left" severity failure;
    assert argt = 0 and crgt = 3 report "'right" severity failure;
    assert ahi = 7 and chi = 3 report "'high" severity failure;
    assert alo = 0 and clo = 0 report "'low" severity failure;
    assert not aasc and casc report "'ascending" severity failure;

    -- The generic's default is two bits, and the second instance gives four
    assert atlen = 2 and ctlen = 4 report "unconstrained generic" severity failure;

    -- Each instance copied its input across its own range
    assert b = "00001111" report "descending copy" severity failure;
    assert d = "1100" report "ascending copy" severity failure;
    assert e = "1111" report "block copy" severity failure;

    report "*-* All Finished *-*";
    stop;
    wait;
  end process;

end architecture;
