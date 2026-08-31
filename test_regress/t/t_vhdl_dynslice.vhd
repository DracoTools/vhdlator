-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- A slice of a value whose length only running decides is another such
-- value (8.5), read and written a place at a time, since which places it
-- picks out is not known until it runs.  Text is carried wherever it
-- goes, so a length taken from a string is one of these too.  Answers
-- from nvc 1.22.1.

library ieee; use ieee.std_logic_1164.all; use ieee.numeric_std.all;
entity t_vhdl_dynslice is
end entity;
architecture rtl of t_vhdl_dynslice is
  function mk(constant kk : integer; constant seed : integer) return unsigned is
    variable z : unsigned(kk - 1 downto 0) := (others => '0');
  begin
    for i in 0 to kk - 1 loop
      if ((seed / (2 ** (i mod 20))) mod 2) = 1 then z(i) := '1'; end if;
    end loop;
    return z;
  end function;
  function pu(constant n : unsigned) return string is
    variable r : unsigned(n'length - 1 downto 0);
  begin
    r := n;
    return integer'image(to_integer(r(7 downto 0))) & ","
      & integer'image(to_integer(r(n'length - 1 downto 4))) & ","
      & integer'image(r(7 downto 4)'length) & ","
      & integer'image(r(7 downto 4)'left) & ","
      & integer'image(to_integer(r(3 downto 3)));
  end function;
  function pw(constant n : unsigned) return string is
    variable r : unsigned(n'length - 1 downto 0);
  begin
    r := n;
    r(7 downto 4) := "1010";
    r(3 downto 0) := r(7 downto 4);
    r(n'length - 1 downto 8) := (others => '0');
    return integer'image(to_integer(r));
  end function;
  -- A local whose length comes from a string, which is always carried
  function from_byte_array(constant byte_array : string) return bit_vector is
    constant b : string(1 to byte_array'length) := byte_array;
    variable ret_val : bit_vector(byte_array'length*8-1 downto 0);
  begin
    for i in b'range loop
      ret_val((b'length-i)*8 + 7 downto (b'length-i)*8)
        := bit_vector(ieee.numeric_bit.to_unsigned(character'pos(b(i)), 8));
    end loop;
    return ret_val;
  end function;
  signal k : integer := 12;
  signal ks : integer := 1234;
begin
  process
    variable v : bit_vector(31 downto 0);
  begin
    -- Reading a slice of a value whose length only running decides
    assert pu(mk(k, 54321)) = "49,67,4,7,0" report "slice read" severity failure;
    -- And writing into one
    assert pw(mk(k, 54321)) = "170" report "slice write" severity failure;
    -- A local whose length comes from a string, which is always carried
    v := from_byte_array(integer'image(ks));
    assert integer'image(v'length) & "," & integer'image(bit'pos(v(7)))
           & "," & integer'image(bit'pos(v(24))) & "," & integer'image(bit'pos(v(25)))
           = "32,0,1,0" report "length from a string" severity failure;

    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
