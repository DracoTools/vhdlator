-- DESCRIPTION: Verilator: VHDL drivers of a record resolved as a whole
--
-- A source of a signal whose composite subtype names a resolution function
-- is a value of the whole composite (14.7.3.2), so a process that assigns
-- only part of it is a driver of all of it. Each element of a waveform is a
-- transaction on that driver (10.5.2.2), and a signal parameter of mode
-- "inout" that the procedure only reads drives nothing (4.2.2.3).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;

entity t_vhdl_recresdrv is
end entity;

architecture test of t_vhdl_recresdrv is
    type rec is record
        a, b : std_logic;
    end record;
    type rec_vec is array (natural range <>) of rec;
    -- '1' wins on each element, whatever else the drivers hold
    function any_one(v : rec_vec) return rec is
        variable r : rec := ('0', '0');
    begin
        for i in v'range loop
            if v(i).a = '1' then r.a := '1'; end if;
            if v(i).b = '1' then r.b := '1'; end if;
        end loop;
        return r;
    end function;
    subtype rrec is any_one rec;
    signal s : rrec;
    signal done : boolean := false;
begin

    p1: process
    begin
        s <= ('0', '0');
        wait until s.a = '1';
        -- Two transactions on one element of this process's driver
        s.b <= '1' after 1 ns, '0' after 2 ns;
        wait;
    end process;

    p2: process
        procedure set_a is
        begin
            s.a <= '1';
        end procedure;
        -- Given the signal for "inout" and only reading it
        procedure await_b(signal x : inout rrec; v : std_logic) is
        begin
            wait until x.b = v;
        end procedure;
    begin
        wait for 1 ns;
        set_a;
        await_b(s, '1');
        assert now = 2 ns severity failure;
        assert s.a = '1' severity failure;
        await_b(s, '0');
        assert now = 3 ns severity failure;
        done <= true;
        wait;
    end process;

    process
    begin
        wait for 10 ns;
        assert done severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
