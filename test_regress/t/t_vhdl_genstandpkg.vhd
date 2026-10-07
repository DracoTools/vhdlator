-- DESCRIPTION: Verilator: VHDL package function given an instance's generic
--
-- A package's function whose variable a scalar parameter sizes, called with
-- an entity's generic for that parameter -- in the generic clause's own
-- defaults and in the architecture -- and a recursive entity handing each
-- level a function of its generic (the shape of NVC issue843, written with
-- an open generic, since GHDL and Questa do not size a formal by another
-- generic of the same map). The copy made for
-- such a call reads the generic where it read the parameter, so the
-- variable is sized per instance and its value folds there.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.math_real.all;

package t_vhdl_genstandpkg_pkg is
    function weights(n : positive) return real_vector;
    function ramp(n : positive) return integer_vector;
    function even_elem(x : integer_vector) return integer_vector;
end package;

package body t_vhdl_genstandpkg_pkg is
    function weights(n : positive) return real_vector is
        variable w : real_vector(0 to n - 1);
    begin
        for k in w'range loop
            w(k) := cos(MATH_2_PI * real(k) / real(n));
        end loop;
        return w;
    end function;
    function ramp(n : positive) return integer_vector is
        variable r : integer_vector(0 to n - 1);
    begin
        for k in r'range loop
            r(k) := 100 * n + k;
        end loop;
        return r;
    end function;
    function even_elem(x : integer_vector) return integer_vector is
        variable res : integer_vector(0 to x'length / 2 - 1);
    begin
        for i in res'range loop
            res(i) := x(x'low + 2 * i);
        end loop;
        return res;
    end function;
end package body;

use work.t_vhdl_genstandpkg_pkg.all;

entity t_vhdl_genstandpkg_w is
    generic (n : positive; w : real_vector(0 to n - 1) := weights(n));
end entity;

architecture a of t_vhdl_genstandpkg_w is
    constant r : integer_vector(0 to n - 1) := ramp(n);
begin
    process is
    begin
        assert abs(w(0) - 1.0) < 1.0e-9 severity failure;
        assert abs(w(n / 2) + 1.0) < 1.0e-9 severity failure;
        assert r(0) = 100 * n and r(n - 1) = 101 * n - 1 severity failure;
        wait;
    end process;
end architecture;

use work.t_vhdl_genstandpkg_pkg.all;

entity t_vhdl_genstandpkg_rec is
    generic (pow : natural; dim : positive := 2 ** pow;
             t : integer_vector := ramp(dim));
end entity;

architecture a of t_vhdl_genstandpkg_rec is
begin
    deeper : if pow > 0 generate
        u : entity work.t_vhdl_genstandpkg_rec generic map (pow => pow - 1, dim => dim / 2, t => even_elem(t));
    end generate;
    process is
    begin
        -- Every level holds the top level's ramp, every other element halved
        -- down: element k is 400 + k * 2 ** (2 - pow)
        for k in t'range loop
            assert t(k) = 400 + k * 2 ** (2 - pow) severity failure;
        end loop;
        wait;
    end process;
end architecture;

entity t_vhdl_genstandpkg is
end entity;

architecture a of t_vhdl_genstandpkg is
begin
    u4 : entity work.t_vhdl_genstandpkg_w generic map (n => 4);
    u8 : entity work.t_vhdl_genstandpkg_w generic map (n => 8);
    r : entity work.t_vhdl_genstandpkg_rec generic map (pow => 2);
    process is
    begin
        wait for 1 ns;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
