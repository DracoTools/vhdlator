-- DESCRIPTION: Verilator: VHDL element resolution written on an array
-- subtype
--
-- "subtype resob is (areso) ivec" resolves each element of an array of
-- INTEGER with areso, and "((areso)) ivec_arr" each element of each element
-- of an array of arrays (6.3); the array itself names no resolution
-- function. The drivers are assigned after a delta wait from other signals,
-- so each one's new value has to reach the resolution (GHDL gna issue2201).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_elemresint_pkg is
    type ivec is array (natural range <>) of integer;
    type ivec_arr is array (natural range <>) of ivec;
    type rint_vec is array (natural range <>) of integer;
    function areso (s : rint_vec) return integer;
    subtype resob is (areso) ivec;
    subtype resoba is ((areso)) ivec_arr;
end package;

package body t_vhdl_elemresint_pkg is
    function areso (s : rint_vec) return integer is
        variable r : integer := 0;
    begin
        for i in s'range loop
            r := r + s(i);
        end loop;
        return r;
    end function;
end package body;

use work.t_vhdl_elemresint_pkg.all;

entity t_vhdl_elemresint is
end entity;

architecture test of t_vhdl_elemresint is
    signal ab : resob(3 downto 0) := (others => 0);
    signal aa : resoba(1 downto 0)(0 to 2) := (others => (others => 0));
    signal v1 : ivec(3 downto 0) := (others => 0);
    signal w1 : ivec_arr(1 downto 0)(0 to 2) := (others => (others => 0));
begin
    p1 : process is
    begin
        wait for 1 ns;
        v1 <= (3 => 1, 2 => 2, 1 => 3, 0 => 4);
        w1 <= (1 => (10, 20, 30), 0 => (40, 50, 60));
        wait for 0 ns;
        ab <= v1;
        aa <= w1;
        wait;
    end process;

    p2 : process is
    begin
        wait for 1 ns;
        ab <= (others => 100);
        aa <= (others => (others => 100));
        wait;
    end process;

    check : process is
    begin
        wait for 2 ns;
        assert ab = (101, 102, 103, 104) report "ab" severity failure;
        assert aa(1) = (110, 120, 130) report "aa(1)" severity failure;
        assert aa(0) = (140, 150, 160) report "aa(0)" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
