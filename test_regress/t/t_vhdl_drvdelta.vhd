-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_drvdelta is
end entity;

architecture test of t_vhdl_drvdelta is

    type nat_array is array (natural range <>) of natural;

    -- Adding them up makes the result the same however the drivers are
    -- ordered, which the language does not say (12.6.2)
    function resolved (x : nat_array) return natural is
        variable sum : natural := 0;
    begin
        for i in x'range loop
            sum := sum + x(i);
        end loop;
        return sum;
    end function;

    subtype rnat is resolved natural;

    signal s : rnat;
begin

    -- A concurrent assignment is the process it stands for: what it gives
    -- its driver is held until the delta ends (11.6)
    p1 : s <= 1;
    p2 : s <= 3;

    chk : process is
    begin
        -- Before any delta the signal is what its own subtype says, not
        -- what the drivers are about to make it
        assert s = 0 report "at 0: " & integer'image(s) severity failure;

        wait for 0 ns;
        assert s = 4 report "after the delta: " & integer'image(s) severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
