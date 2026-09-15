-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

entity t_vhdl_elemresolve is
end entity;

architecture test of t_vhdl_elemresolve is
    -- 14.7.3.2: a composite signal whose own subtype names no resolution
    -- function is still resolved where its element subtype names one. The
    -- drivers are resolved one element at a time, and the function is run
    -- once per element over that element's values.
    function and_bits (s : bit_vector) return bit is
        variable r : bit := '1';
    begin
        for i in s'range loop
            r := r and s(i);
        end loop;
        return r;
    end function;

    subtype rbit is and_bits bit;
    type rbit_vector is array (natural range <>) of rbit;

    signal v : rbit_vector(0 to 3);
    -- The same element subtype reached through an array of a named length
    signal w : rbit_vector(3 downto 0);
begin

    v <= "1100";
    v <= "1010";

    w <= "1110";
    w <= "0110";
    w <= "1111";

    main : process is
    begin
        wait for 1 ns;
        assert v = "1000"
            report "two drivers resolved element by element" severity failure;
        assert w = "0110"
            report "three drivers, and a descending index range" severity failure;

        report "*-* All Finished *-*";
        std.env.stop;
    end process;
end architecture;
