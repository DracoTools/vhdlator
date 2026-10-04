-- DESCRIPTION: Verilator: VHDL loop over the range of a function result
--
-- The result subtype of the function names its parameter, so the bounds
-- are known only once the call is made; the loop takes them from the
-- value the call returns.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_callrange is
end entity;

architecture test of t_vhdl_callrange is
    type desc_t is record
        address : natural;
        length  : positive;
    end record;

    function span (d : desc_t) return bit_vector is
        variable v : bit_vector(d.address to d.address + d.length);
    begin
        return v;
    end function;

    function span_down (d : desc_t) return bit_vector is
        variable v : bit_vector(d.address + d.length downto d.address);
    begin
        return v;
    end function;

    -- The same loop inside a subprogram
    function total (d : desc_t) return natural is
        variable s : natural := 0;
    begin
        for i in span(d)'range loop
            s := s + i;
        end loop;
        return s;
    end function;

    signal sd : desc_t := (3, 2);
begin
    process is
        variable d     : desc_t := (8, 8);
        variable n     : natural;
        variable first : integer;
        variable last  : integer;
    begin
        n := 0;
        first := -1;
        for i in span(d)'range loop
            if first < 0 then
                first := i;
            end if;
            last := i;
            n := n + 1;
        end loop;
        assert n = 9 and first = 8 and last = 16 severity failure;

        n := 0;
        first := -1;
        for i in span(d)'reverse_range loop
            if first < 0 then
                first := i;
            end if;
            last := i;
            n := n + 1;
        end loop;
        assert n = 9 and first = 16 and last = 8 severity failure;

        first := -1;
        for i in span_down(d)'range loop
            if first < 0 then
                first := i;
            end if;
            last := i;
        end loop;
        assert first = 16 and last = 8 severity failure;

        -- The bounds follow the argument from one call to the next
        d := (0, 3);
        n := 0;
        for i in span(d)'range loop
            n := n + 1;
        end loop;
        assert n = 4 severity failure;

        assert total((1, 2)) = 6 severity failure;
        wait for 1 ns;
        assert total(sd) = 3 + 4 + 5 severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
