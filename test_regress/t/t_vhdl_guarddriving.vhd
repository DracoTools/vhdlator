-- DESCRIPTION: Verilator: VHDL 'driving of a guarded signal (16.2.4)
--
-- A driver of a guarded signal is disconnected by a null transaction and
-- connected again by any other (10.5.2.2); S'DRIVING says which it is in
-- now, for the process that asks.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_guarddriving is
end entity;

architecture test of t_vhdl_guarddriving is
    type int_vec is array (natural range <>) of integer;

    -- How many drivers are connected
    function count(x : int_vec) return integer is
    begin
        return x'length;
    end function;

    subtype cint is count integer;

    signal vbus : cint bus;
    signal vreg : cint register;
begin

    process is
    begin
        assert vbus = 1 severity failure;
        assert vbus'driving severity failure;
        vbus <= 100;
        wait for 1 ns;
        assert vbus = 1 severity failure;
        assert vbus'driving severity failure;
        vbus <= null;
        -- Still connected until the null transaction matures
        assert vbus'driving severity failure;
        wait for 1 ns;
        assert vbus = 0 severity failure;
        assert not vbus'driving severity failure;
        vbus <= 5 after 1 ns;
        wait for 2 ns;
        assert vbus = 1 severity failure;
        assert vbus'driving severity failure;

        assert vreg'driving severity failure;
        vreg <= null;
        wait for 1 ns;
        -- A register keeps the value it had when its last driver went
        assert vreg = 1 severity failure;
        assert not vreg'driving severity failure;

        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
