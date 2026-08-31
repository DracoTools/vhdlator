-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- numeric_std declares its operators over unconstrained operands, so a
-- value whose length only running decides reaches all of them.  Every
-- answer below was taken from nvc 1.22.1, including the 40-bit division,
-- which is wider than any machine word this compiles to.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_dynarith is
end entity;

architecture rtl of t_vhdl_dynarith is

  -- A value of a length nothing but running knows
  function mku(constant kk : integer; constant seed : integer) return unsigned is
    variable z : unsigned(kk - 1 downto 0) := (others => '0');
  begin
    for i in 0 to kk - 1 loop
      if ((seed / (2 ** (i mod 20))) mod 2) = 1 then z(i) := '1'; end if;
    end loop;
    return z;
  end function;

  function mks(constant kk : integer; constant seed : integer) return signed is
    variable z : signed(kk - 1 downto 0) := (others => '0');
  begin
    for i in 0 to kk - 1 loop
      if ((seed / (2 ** (i mod 20))) mod 2) = 1 then z(i) := '1'; end if;
    end loop;
    return z;
  end function;

  function pu(constant n : unsigned; constant m : unsigned) return string is
    variable r : unsigned(n'length - 1 downto 0);
    variable sm : unsigned(n'length - 1 downto 0);
    variable sh : unsigned(n'length - 1 downto 0);
  begin
    r := n;
    sm := r + m;
    sh := r sll 1;
    return integer'image(to_integer(r sll 3)) & "," & integer'image(to_integer(r srl 2))
      & "," & integer'image(to_integer(r rol 5)) & "," & integer'image(to_integer(r ror 3))
      & "," & integer'image(to_integer(r + 7)) & "," & integer'image(to_integer(r - 9))
      & "," & integer'image(to_integer(r / 10)) & "," & integer'image(to_integer(r mod 10))
      & "," & integer'image(to_integer(r rem 7))
      & "," & integer'image(to_integer(r + m)) & "," & integer'image(to_integer(r - m))
      & "," & boolean'image(r > m) & "," & boolean'image(r <= m)
      & "," & boolean'image(r >= 100) & "," & boolean'image(r < 100)
      & "," & integer'image(sm'length) & "," & integer'image(sh'left);
  end function;

  function ps(constant n : signed; constant m : signed) return string is
    variable r : signed(n'length - 1 downto 0);
  begin
    r := n;
    return integer'image(to_integer(r + 7)) & "," & integer'image(to_integer(r - 9))
      & "," & integer'image(to_integer(r + m)) & "," & integer'image(to_integer(r - m))
      & "," & boolean'image(r > m) & "," & boolean'image(r <= m)
      & "," & boolean'image(r >= -5) & "," & boolean'image(r < 3);
  end function;

  -- Long division of a value wider than a machine word
  function pl(constant n : unsigned) return string is
    variable r : unsigned(n'length - 1 downto 0);
    variable q : unsigned(n'length - 1 downto 0);
    variable m : unsigned(n'length - 1 downto 0);
  begin
    r := n;
    q := r / 10;
    m := r mod 10;
    return integer'image(to_integer(m)) & "," & integer'image(to_integer(q srl 24))
      & "," & integer'image(to_integer((r - 12345) srl 24))
      & "," & integer'image(to_integer((r sll 4) srl 28));
  end function;

  signal k12 : integer := 12;
  signal k9 : integer := 9;
  signal k28 : integer := 28;
  signal k10 : integer := 10;
  signal k14 : integer := 14;
  signal k40 : integer := 40;

begin
  process
  begin
    assert pu(mku(k12, 1234), mku(k9, 300))
      = "1680,308,2633,1178,1241,1225,123,4,2,1534,934,true,false,true,false,12,11"
      report "12-bit unsigned wrong" severity failure;
    assert pu(mku(k28, 987654), mku(k28, 55))
      = "58232880,1819777,232931520,202236480,7279117,7279101,727911,0,6,"
        & "64950845,218042831,false,true,true,false,28,27"
      report "28-bit unsigned wrong" severity failure;
    assert pl(mku(k40, 987654)) = "8,6172,61728,288"
      report "40-bit division wrong" severity failure;
    assert ps(mks(k12, 1234), mks(k9, 300)) = "1241,1225,1022,1446,true,false,true,false"
      report "12-bit signed wrong" severity failure;
    assert ps(mks(k10, 999), mks(k14, 77)) = "-18,-34,52,-102,false,true,false,true"
      report "10-bit signed wrong" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
