-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A signal parameter whose subtype fixes its length, given an actual of another length
-- through a procedure that takes any length, is an error when the call is made (4.2.2.1).

entity t_vhdl_sigparam_len_bad is
end entity;

architecture a of t_vhdl_sigparam_len_bad is
  procedure check (signal v : bit_vector(1 to 3)) is
  begin
    report "length " & integer'image(v'length);
  end procedure;
  procedure pass_on (signal w : bit_vector) is
  begin
    check(w);
  end procedure;
  signal s3 : bit_vector(1 to 3) := "101";
  signal s2 : bit_vector(1 to 2) := "01";
begin
  process
  begin
    pass_on(s3);
    pass_on(s2);
    report "not reached" severity failure;
    wait;
  end process;
end architecture;
