-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity t_vhdl_callwidth is
end entity;

architecture test of t_vhdl_callwidth is

    -- A subprogram whose parameter has no length of its own is emitted once
    -- per call site with the length that call supplies (4.2.2.1). The call
    -- has to be able to say it.
    procedure show (v : signed; expect : integer) is
    begin
        assert v'length = expect
            report "length " & integer'image(v'length) & " wanted "
                   & integer'image(expect)
            severity failure;
    end procedure;

    procedure showu (v : unsigned; expect : integer) is
    begin
        assert v'length = expect report "unsigned length" severity failure;
    end procedure;

    signal d : integer := 5;
begin

    main : process is
        variable u : unsigned(7 downto 0) := X"A5";
    begin
        -- 16.5: these leave the result subtype open and say the length in
        -- the call itself
        show(to_signed(d, 32), 32);
        show(to_signed(d, 8), 8);
        showu(to_unsigned(3, 12), 12);
        showu(resize(u, 16), 16);
        showu(shift_left(u, 2), 8);
        showu(rotate_right(u, 3), 8);

        -- And the values themselves survive the round trip
        assert to_integer(to_signed(-7, 16)) = -7 report "value" severity failure;
        assert resize(u, 16) = X"00A5" report "resized value" severity failure;
        assert shift_left(u, 1) = X"4A" report "shifted value" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
