-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_capsib is
end entity;

architecture a of t_vhdl_capsib is

    -- Three nested subprograms, each naming a different one of the outer's
    -- own objects, and calling each other in a chain. What the last one
    -- reaches out for has to be handed all the way down, through callers
    -- that never mention it themselves.
    function decorate (msg : string; tag : string; mark : character) return string is

        function put_mark return string is
        begin
            return "" & mark;
        end function;

        function put_tag return string is
        begin
            return "<" & tag & put_mark & ">";
        end function;

        function put_msg return string is
            variable acc : string(1 to msg'length + tag'length + 3);
            variable at : natural := 1;
        begin
            for i in msg'range loop
                acc(at) := msg(i);
                at := at + 1;
            end loop;
            for i in put_tag'range loop
                acc(at) := put_tag(i);
                at := at + 1;
            end loop;
            return acc;
        end function;
    begin
        return put_msg;
    end function;
begin
    process
        constant one : string := decorate("ab", "HERE", '!');
        constant two : string := decorate("xyz", "Z", '?');
    begin
        assert one = "ab<HERE!>" report "one = " & one severity failure;
        assert two = "xyz<Z?>" report "two = " & two severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
