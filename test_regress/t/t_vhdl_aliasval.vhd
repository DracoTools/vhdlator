-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain, for
-- any use, without warranty, 2026 by Wilson Snyder.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package p is
    -- A constant whose value the body gives (4.7)
    constant deferred : integer;
end package;

package body p is
    constant base : integer := 7;
    alias named : integer is base;
    constant deferred : integer := named + 1;
end package body;

use work.p.all;

entity t_vhdl_aliasval is
end entity;

architecture test of t_vhdl_aliasval is
    signal v : bit_vector(0 to 1) := "00";
    alias v0 : bit is v(0);
    signal s : bit := '0';
    alias sa : bit is s;

    procedure drive(signal value : out bit) is
    begin
        value <= '1';
    end procedure;
begin

    process is
    begin
        assert deferred = 8 report "deferred" severity failure;
        drive(sa);
        drive(v0);
        wait for 1 ns;
        assert s = '1' report "s" severity failure;
        assert v(0) = '1' report "v0" severity failure;
        assert v(1) = '0' report "v1" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
