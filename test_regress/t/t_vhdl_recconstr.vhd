-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_recconstr is
end entity;

architecture rtl of t_vhdl_recconstr is
  type bv_array_t is array (natural range <>) of bit_vector;
  type rec_t is record
    f : bv_array_t;
    n : integer;
  end record;

  -- A record constraint may say how long an element is and how long *its*
  -- element is, and the two are written one after the other (5.3.3)
  signal s : rec_t(f(1 to 3)(1 to 2));
  subtype named_t is rec_t(f(0 to 1)(3 downto 0));
  signal t : named_t;

  -- "open" says nothing about that dimension: it stays open where the base
  -- type left it open, and stays as it was where the base already fixed it
  type open_t is array (natural range <>) of bit_vector;
  subtype byte_arr_t is open_t(open)(7 downto 0);
  type fixed_t is array (0 to 2) of bit_vector;
  subtype fixed_byte_t is fixed_t(open)(3 downto 0);
  signal u : byte_arr_t(1 to 2);
  signal w : fixed_byte_t;
begin
  main : process is
    variable v : rec_t(f(1 to 2)(7 downto 0));
  begin
    assert s.f'length = 3 report "s.f length" severity failure;
    assert s.f(1)'length = 2 report "s.f element length" severity failure;
    assert t.f'length = 2 report "t.f length" severity failure;
    assert t.f(0)'length = 4 report "t.f element length" severity failure;
    assert v.f(1)'left = 7 report "v.f element left" severity failure;
    assert u'length = 2 report "u length" severity failure;
    assert u(1)'length = 8 report "u element length" severity failure;
    assert w'length = 3 report "w length" severity failure;
    assert w(0)'length = 4 report "w element length" severity failure;
    u <= (X"12", X"34");
    w <= (X"5", X"6", X"7");
    s.f <= ("10", "01", "11");
    v.f := (X"A5", X"5A");
    wait for 1 ns;
    assert s.f(2) = "01" report "s.f value" severity failure;
    assert v.f(2) = X"5A" report "v.f value" severity failure;
    assert u(2) = X"34" report "u value" severity failure;
    assert w(2) = X"7" report "w value" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
