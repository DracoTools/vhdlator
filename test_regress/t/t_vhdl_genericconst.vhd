-- DESCRIPTION: Verilator: VHDL generic given an array constant, or a function of one
--
-- What an instance hands a generic is settled by elaboration (6.5.6.2): an
-- array constant by name, a slice of it, or what a function makes of it.
-- An unconstrained generic takes its bounds from that value, which for a
-- call is only known once the call has been given its own copy.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_genericconst_pkg is
    function half (v : bit_vector) return bit_vector;
    function ones (v : bit_vector) return natural;
end package;

package body t_vhdl_genericconst_pkg is
    function half (v : bit_vector) return bit_vector is
        variable r : bit_vector(0 to v'length / 2 - 1);
    begin
        for i in r'range loop
            r(i) := v(v'low + 2 * i);
        end loop;
        return r;
    end function;

    function ones (v : bit_vector) return natural is
        variable n : natural := 0;
    begin
        for i in v'range loop
            if v(i) = '1' then
                n := n + 1;
            end if;
        end loop;
        return n;
    end function;
end package body;

-- A constrained generic
entity t_vhdl_genericconst_fix is
    generic (w : bit_vector(0 to 3));
    port (y : out bit_vector(0 to 3));
end entity;

architecture rtl of t_vhdl_genericconst_fix is
begin
    y <= w;
end architecture;

-- An unconstrained one, whose bounds the instance settles
entity t_vhdl_genericconst_open is
    generic (w : bit_vector);
    port (y : out bit_vector(0 to w'length - 1);
          lo, hi : out integer;
          up : out boolean);
end entity;

architecture rtl of t_vhdl_genericconst_open is
begin
    y <= w;
    lo <= w'low;
    hi <= w'high;
    up <= w'ascending;
end architecture;

-- A number worked out from an array constant
entity t_vhdl_genericconst_num is
    generic (n : natural);
    port (y : out bit_vector(0 to n - 1));
end entity;

architecture rtl of t_vhdl_genericconst_num is
begin
    y <= (others => '1');
end architecture;

use work.t_vhdl_genericconst_pkg.all;

entity t_vhdl_genericconst is
end entity;

architecture test of t_vhdl_genericconst is
    constant c : bit_vector(0 to 3) := "1101";
    constant d : bit_vector(7 downto 4) := "0110";
    constant h : bit_vector := half(c);
    signal y_fix, y_open, y_down : bit_vector(0 to 3);
    signal y_slice, y_call, y_lit, y_const : bit_vector(0 to 1);
    signal y_num : bit_vector(0 to 2);
    signal lo, hi, dlo, dhi : integer;
    signal up, dup : boolean;
begin
    u_fix : entity work.t_vhdl_genericconst_fix
        generic map (w => c) port map (y => y_fix);
    u_open : entity work.t_vhdl_genericconst_open
        generic map (w => c) port map (y => y_open, lo => lo, hi => hi, up => up);
    u_down : entity work.t_vhdl_genericconst_open
        generic map (w => d) port map (y => y_down, lo => dlo, hi => dhi, up => dup);
    u_slice : entity work.t_vhdl_genericconst_open
        generic map (w => c(0 to 1)) port map (y => y_slice, lo => open, hi => open, up => open);
    u_call : entity work.t_vhdl_genericconst_open
        generic map (w => half(c)) port map (y => y_call, lo => open, hi => open, up => open);
    u_lit : entity work.t_vhdl_genericconst_open
        generic map (w => half("0111")) port map (y => y_lit, lo => open, hi => open, up => open);
    u_const : entity work.t_vhdl_genericconst_open
        generic map (w => h) port map (y => y_const, lo => open, hi => open, up => open);
    u_num : entity work.t_vhdl_genericconst_num
        generic map (n => ones(c)) port map (y => y_num);

    process is
    begin
        wait for 1 ns;
        assert y_fix = "1101" severity failure;
        assert y_open = "1101" severity failure;
        assert lo = 0 and hi = 3 and up severity failure;
        assert y_down = "0110" severity failure;
        assert dlo = 4 and dhi = 7 and not dup severity failure;
        assert y_slice = "11" severity failure;
        assert y_call = "10" severity failure;
        assert y_lit = "01" severity failure;
        assert y_const = "10" severity failure;
        assert y_num = "111" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
