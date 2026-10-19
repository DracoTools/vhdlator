-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A record signal parameter whose elements are given one at a time: an element whose
-- subtype fixes its length, given an actual of another length, is an error when the call
-- is made (4.2.2.1; GHDL gna bug077).

entity t_vhdl_sigparam_elemlen_bad is
end entity;

architecture a of t_vhdl_sigparam_elemlen_bad is
  type rec is record
    a : bit;
    w : bit_vector(1 to 3);
  end record;
  procedure check (signal v : rec) is
  begin
    report "w " & integer'image(v.w'length);
  end procedure;
  procedure pack (signal a : bit; signal w : bit_vector) is
  begin
    check(v.a => a, v.w => w);
  end procedure;
  signal sa : bit;
  signal s3 : bit_vector(1 to 3) := "001";
  signal s2 : bit_vector(1 to 2) := "01";
begin
  process
  begin
    pack(sa, s3);
    pack(sa, s2);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
