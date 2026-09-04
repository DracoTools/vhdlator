-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
library std;
use std.textio.all;

entity t_vhdl_image is
end entity;

architecture test of t_vhdl_image is
    -- A value whose length only running decides, so its text cannot come
    -- from a conversion of a known width
    function ramp (constant n : natural) return std_logic_vector is
        variable r : std_logic_vector(n - 1 downto 0) := (others => '0');
    begin
        for i in r'range loop
            if (i mod 3) = 0 then
                r(i) := '1';
            end if;
        end loop;
        return r;
    end function;

    procedure same (constant got : string; constant want : string) is
    begin
        if got /= want then
            report "%Error: got [" & got & "] wanted [" & want & "]" severity failure;
        end if;
    end procedure;

    signal s : std_logic_vector(7 downto 0) := x"42";
begin
    process
        variable l : line;
        variable c : character := 'x';
        variable t : time := 3 ns;
    begin
        -- TO_STRING of a physical value is its image, unit and all (5.7)
        same(to_string(t), "3000000 fs");
        same(time'image(t), "3000000 fs");
        same(to_string(t, ns), "3 ns");

        -- Only the image of a character literal keeps its apostrophes
        same(to_string(c), "x");
        same(character'image(c), "'x'");
        same("<" & c & ">", "<x>");

        -- TEXTIO writes a time in a unit, NS unless the call names one (16.4)
        wait for 2 ns;
        write(l, now);
        same(l.all, "2 ns");
        deallocate(l);
        write(l, now, left, 0, ps);
        same(l.all, "2000 ps");
        deallocate(l);

        -- A known width reads the same either way
        same(to_hstring(s), "42");
        same(to_string(s), "01000010");

        -- and so does one that is only known while running
        same(to_hstring(ramp(1)), "1");
        same(to_hstring(ramp(4)), "9");
        same(to_hstring(ramp(7)), "49");
        same(to_hstring(ramp(10)), "249");
        same(to_ostring(ramp(4)), "11");
        same(to_ostring(ramp(10)), "1111");
        same(to_string(ramp(10)), "1001001001");

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
