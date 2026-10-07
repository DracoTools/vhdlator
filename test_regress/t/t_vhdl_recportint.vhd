-- DESCRIPTION: Verilator: VHDL port of a record whose open element is an
-- array of integers
--
-- A port of a record type with an element of an unconstrained array of
-- integers takes the element's bounds from the actual (5.3.3, 6.5.6.3), as
-- one of bits does. Two instances of different lengths, the port's element
-- read through 'subtype, 'range and 'reverse_range, and copied to an out
-- port of the same record type.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_recportint_pkg is
    type rec_t is record
        x : integer_vector;
        k : integer;
    end record;
end package;

use work.t_vhdl_recportint_pkg.all;

entity t_vhdl_recportint_sub is
    port (p : in rec_t; o : out integer; q : out rec_t);
end entity;

architecture test of t_vhdl_recportint_sub is
begin
    process (p) is
        variable local_s : p'subtype;
        variable result : integer := 0;
    begin
        local_s := p;
        result := 0;
        for i in local_s.x'reverse_range loop
            if local_s.x(i) > 0 then
                result := local_s.x(i) + local_s.k;
                exit;
            end if;
        end loop;
        o <= result;
        q <= p;
    end process;
end architecture;

use work.t_vhdl_recportint_pkg.all;

entity t_vhdl_recportint is
end entity;

architecture test of t_vhdl_recportint is
    signal s5 : rec_t(x(1 to 5)) := (x => (1, 2, 3, 0, 0), k => 0);
    signal s3 : rec_t(x(7 downto 5)) := (x => (4, 0, 0), k => 10);
    signal q5 : rec_t(x(1 to 5));
    signal q3 : rec_t(x(7 downto 5));
    signal o5, o3 : integer;
begin
    u5 : entity work.t_vhdl_recportint_sub port map (s5, o5, q5);
    u3 : entity work.t_vhdl_recportint_sub port map (p => s3, o => o3, q => q3);

    check : process is
    begin
        wait for 1 ns;
        assert o5 = 3 severity failure;
        assert o3 = 14 severity failure;
        assert q5.x = (1, 2, 3, 0, 0) and q3.x(7) = 4 severity failure;
        s5.x(4) <= 222;
        s3.x(5) <= 6;
        s3.k <= 1;
        wait for 1 ns;
        assert o5 = 222 severity failure;
        assert o3 = 7 severity failure;
        assert q5.x(4) = 222 and q3.x(5) = 6 and q3.k = 1 severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
