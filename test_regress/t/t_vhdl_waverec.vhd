-- DESCRIPTION: Verilator: VHDL projected waveform, a record signal
--
-- Each scalar subelement of a record has its projected waveform: a whole-record transaction preempts and rejects per subelement, and a field, a nested field or an element preempts its own part (GHDL and Questa agree; nvc rejects the record as a whole).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
entity t_vhdl_waverec is end entity;
architecture a of t_vhdl_waverec is
  type inner_t is record
    flag : std_logic;
    code : std_logic_vector(3 downto 0);
  end record;
  type rec_t is record
    a : integer;
    b : integer;
    n : inner_t;
  end record;
  signal r : rec_t := (0, 0, ('0', x"0"));
begin
  process
  begin
    -- Whole record, inertial: the second preempts the first
    r <= (1, 1, ('1', x"1")) after 10 ns;
    r <= (2, 2, ('0', x"2")) after 5 ns;
    wait for 20 ns;
    assert r.a = 2 and r.b = 2 and r.n.code = x"2" report "1" severity failure;
    -- Rejection per subelement: within the window, a pending value equal
    -- to the new one stays, another goes
    r <= (7, 3, ('0', x"2")) after 3 ns;   -- due at 23 ns
    wait for 1 ns;
    r <= (7, 9, ('0', x"2")) after 4 ns;   -- due at 25 ns: a keeps its 7 at 23, b's 3 is rejected
    wait for 3 ns;                          -- 24 ns
    assert r.a = 7 report "2 a=" & integer'image(r.a) severity failure;
    assert r.b = 2 report "3 b=" & integer'image(r.b) severity failure;
    wait for 2 ns;                          -- 26 ns
    assert r.b = 9 report "4" severity failure;
    -- A field, a nested field, an element of a nested vector
    r.a <= 11 after 10 ns;
    r.a <= 12 after 5 ns;
    r.n.flag <= '1' after 2 ns;
    r.n.code(0) <= '1' after 2 ns;
    wait for 12 ns;
    assert r.a = 12 report "5 a=" & integer'image(r.a) severity failure;
    assert r.n.flag = '1' and r.n.code = x"3" report "6" severity failure;
    -- A field preempts its part of a whole transaction
    r <= (0, 0, ('0', x"0")) after 10 ns;
    r.b <= 5 after 5 ns;
    wait for 12 ns;
    assert r.a = 0 and r.b = 5 report "7" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
