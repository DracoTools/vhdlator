-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A STRING variable given a value of another length is an error when it
-- runs (10.6.2.1), where the variable's length or the value's is one only
-- running settles. Values of the right length pass without a word.

entity t_vhdl_strlenrun_bad is
end entity;

architecture test of t_vhdl_strlenrun_bad is
    function cat(a, b : string) return string is
    begin
        return a & b;
    end function;

    -- Held as long as the call says
    procedure sized(n : natural; x : string; expect : string) is
        variable s : string(1 to n);
    begin
        s := cat(x, "cd");
        -- Only a value of the right length has a value to check
        if expect'length = n then
            assert s = expect report "sized gave " & s severity failure;
        end if;
    end procedure;
begin
    process
        variable five : string(1 to 5);
    begin
        sized(5, "abc", "abccd");
        -- A fixed length, given a value only running measures
        five := cat("xyz", "uv");
        assert five = "xyzuv" report "five gave " & five severity failure;
        report "lengths that match passed";
        sized(5, "abcde", "abcdecd");
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
