-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A function that gives back an array of vectors, where how many there are
-- and how long each is both come from what it was handed, called where only
-- running says either. The function had no body at all, and the call was
-- refused. Behind that: an object that carries its bounds, written whole,
-- took the bounds of the value it was given rather than keeping its own
-- (10.6.2.1); and what an access value points at, handed to such a
-- parameter that the procedure writes, was given a copy the writes never
-- came back from. UVVM's axistream_receive is the shape (bitvis_vip_
-- axistream/src/axistream_bfm_pkg.vhd, convert_byte_array_to_slv_array).
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_retslvarr is
end entity;

architecture a of t_vhdl_retslvarr is
  type t_slv_array is array (natural range <>) of std_logic_vector;
  type t_slv_array_ptr is access t_slv_array;
  subtype t_byte_array is t_slv_array(open)(7 downto 0);

  -- Bytes packed into words, running the way the bytes run
  impure function conv(constant ba : t_byte_array; constant bpw : natural)
    return t_slv_array is
    constant n : integer := ba'length / bpw;
    variable va : t_slv_array(0 to n - 1)(8 * bpw - 1 downto 0);
    variable vd : t_slv_array(n - 1 downto 0)(8 * bpw - 1 downto 0);
    variable k : integer := 0;
  begin
    for i in 0 to n - 1 loop
      for b in bpw downto 1 loop
        va(i)(8 * b - 1 downto (b - 1) * 8) := ba(ba'low + k);
        vd(i)(8 * b - 1 downto (b - 1) * 8) := ba(ba'low + k);
        k := k + 1;
      end loop;
    end loop;
    if ba'ascending then
      return va;
    else
      return vd;
    end if;
  end function;

  -- Fills the words it is handed, however many there are and however long
  procedure recv(variable da : inout t_slv_array; variable lft : out integer) is
    constant bpw : natural := da(da'low)'length / 8;
    variable vb : t_slv_array(0 to da'length * bpw - 1)(7 downto 0);
  begin
    for i in vb'range loop
      vb(i) := std_logic_vector(to_unsigned(i + 1, 8));
    end loop;
    da := conv(vb, bpw);
    lft := da'left;
  end procedure;

  procedure expect(constant ex : t_slv_array; variable ok : out boolean;
                   variable lft : out integer) is
    variable rx : t_slv_array(ex'range)(ex(ex'low)'range);
  begin
    recv(rx, lft);
    ok := rx(rx'left) = ex(ex'left) and rx(rx'right) = ex(ex'right);
  end procedure;

  -- Gives a call's result straight back
  impure function words(constant ba : t_byte_array) return t_slv_array is
  begin
    return conv(ba, 2);
  end function;

  -- A value of one length written into an object of another range
  function reversed(v : bit_vector) return bit_vector is
    variable r : bit_vector(0 to v'length - 1);
  begin
    for i in r'range loop
      r(i) := v(v'low + i);
    end loop;
    return r;
  end function;

  procedure flip(variable v : inout bit_vector; variable l, r : out integer) is
  begin
    v := reversed(v);
    l := v'left;
    r := v'right;
  end procedure;
begin
  process
    variable big : t_slv_array(0 to 7)(15 downto 0)
      := (x"0102", x"0304", x"0506", x"0708", others => x"0000");
    variable cnt : natural := 3;
    variable w : natural := 24;
    variable ok : boolean;
    variable lft, rgt : integer;
    variable p : t_slv_array_ptr;
    variable b4 : t_byte_array(1 to 4) := (x"11", x"22", x"33", x"44");
    variable r2 : t_slv_array(5 to 6)(15 downto 0);
    variable bv : bit_vector(15 downto 0) := x"00f1";
  begin
    expect(big(0 to cnt - 1), ok, lft);
    assert ok and lft = 0 severity failure;
    cnt := 4;
    -- Written with a range starting at 1; the value starts at 0
    expect(big(1 to cnt - 1), ok, lft);
    assert lft = 1 and not ok severity failure;
    -- Neither the count nor the width is known until running
    p := new t_slv_array(1 to cnt - 1)(w - 1 downto 0);
    recv(p.all, lft);
    assert p(1) = x"010203" and p(3) = x"070809" and lft = 1 severity failure;
    assert p'left = 1 and p'right = 3 and p(2)'length = 24 severity failure;
    deallocate(p);
    r2 := words(b4);
    assert r2(5) = x"1122" and r2(6) = x"3344" severity failure;
    -- One level: the object keeps "7 downto 0"
    flip(bv(cnt + 3 downto cnt - 4), lft, rgt);
    assert lft = 7 and rgt = 0 severity failure;
    assert bv = x"008f" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
