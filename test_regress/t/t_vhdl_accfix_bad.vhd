-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- What an access value points at, four elements, assigned to a variable of
-- three: the lengths must match, which only running can check (10.6.2.1)
entity t_vhdl_accfix_bad is
end entity;

architecture test of t_vhdl_accfix_bad is
    type int_vec is array (natural range <>) of integer;
    type int_vec_ptr is access int_vec;
begin
    process
        variable pi : int_vec_ptr;
        variable three : int_vec(0 to 2);
    begin
        pi := new int_vec'(1, 2, 3, 4);
        three := pi.all;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
