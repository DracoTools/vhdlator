-- DESCRIPTION: Verilator: VHDL 'LAST_VALUE of a composite signal compared with an aggregate
--
-- The aggregate takes its type from the other operand, which here is the
-- value the signal had before its last event.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_lastvalagg is
end entity;

architecture test of t_vhdl_lastvalagg is
    type t_rec is record
        x : integer;
        y : real;
    end record;

    type t_array is array (natural range <>) of t_rec;

    signal s : t_rec := (6, 42.0);
    signal t : t_array(1 to 3) := (others => (0, 0.0));
begin

    process is
    begin
        assert s'last_value = (6, 42.0) severity failure;
        assert t'last_value(1) = (0, 0.0) severity failure;
        s <= (4, 22.0);
        wait for 1 ns;
        assert s'last_value.x = 6 severity failure;
        assert s'last_value = (6, 42.0) severity failure;
        assert s'last_value /= (4, 22.0) severity failure;
        assert s = (4, 22.0) severity failure;
        t(3) <= (8, 55.0);
        wait for 1 ns;
        assert t = ((0, 0.0), (0, 0.0), (8, 55.0)) severity failure;
        assert t'last_value = (1 to 3 => (0, 0.0)) severity failure;
        assert (1 to 3 => (0, 0.0)) = t'last_value severity failure;
        assert t'last_value /= ((0, 0.0), (0, 0.0), (8, 55.0)) severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
