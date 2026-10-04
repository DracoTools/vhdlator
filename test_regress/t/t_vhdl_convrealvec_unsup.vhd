-- DESCRIPTION: Verilator: VHDL conversion between arrays of INTEGER and REAL
-- whose operand cannot be taken element by element
--
-- The conversion is made one element at a time (t_vhdl_convrealvec), which
-- needs the operand's length here and an operand that can be read once per
-- element. A concatenation is neither yet.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_convrealvec_unsup is
end entity;

architecture test of t_vhdl_convrealvec_unsup is
begin
    process is
        variable i : integer_vector(1 to 3) := (4, 6, -8);
        variable r : real_vector(1 to 4);
    begin
        r := real_vector(i & 9);
        wait;
    end process;
end architecture;
