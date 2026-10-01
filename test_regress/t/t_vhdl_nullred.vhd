-- DESCRIPTION: Verilator: VHDL reduction operators over a null slice
--
-- A slice whose ends run the wrong way round has no elements (5.2.1), and a
-- reduction operator over it gives its identity (9.2.8).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.fixed_pkg.all;

entity t_vhdl_nullred is
end entity;

architecture test of t_vhdl_nullred is

    -- As ieee.fixed_pkg's HREAD asks whether the bits above the value read
    -- are all zero: at some widths there are none
    procedure p (value : in ufixed; fill : in std_ulogic;
                 o_and, o_or, o_xor, o_nand, o_nor, o_xnor : out std_ulogic) is
        constant hbv : integer := (((maximum(4, (value'high + 1)) + 3) / 4) * 4) - 1;
        constant lbv : integer := ((minimum(0, value'low) - 3) / 4) * 4;
        variable slv : std_ulogic_vector(hbv - lbv downto 0) := (others => fill);
    begin
        o_and := and (slv(hbv - lbv downto value'high + 1 - lbv));
        o_or := or (slv(hbv - lbv downto value'high + 1 - lbv));
        o_xor := xor (slv(hbv - lbv downto value'high + 1 - lbv));
        o_nand := nand (slv(hbv - lbv downto value'high + 1 - lbv));
        o_nor := nor (slv(hbv - lbv downto value'high + 1 - lbv));
        o_xnor := xnor (slv(hbv - lbv downto value'high + 1 - lbv));
    end procedure;

begin

    process is
        variable a : ufixed(3 downto -4);    -- nothing above: a null slice
        variable b : ufixed(31 downto -4);   -- the same, wider
        variable c : ufixed(2 downto -4);    -- one bit above
        variable d : ufixed(1 downto -4);    -- two bits above
        variable r : std_ulogic_vector(1 to 6);
        variable up : std_ulogic_vector(0 to 7) := (others => '1');
    begin
        for f in 0 to 1 loop
            p(a, std_ulogic'val(2 + f), r(1), r(2), r(3), r(4), r(5), r(6));
            assert r = "100011" report "a" severity failure;
            p(b, std_ulogic'val(2 + f), r(1), r(2), r(3), r(4), r(5), r(6));
            assert r = "100011" report "b" severity failure;
        end loop;
        p(c, '1', r(1), r(2), r(3), r(4), r(5), r(6));
        assert r = "111000" report "c1" severity failure;
        p(c, '0', r(1), r(2), r(3), r(4), r(5), r(6));
        assert r = "000111" report "c0" severity failure;
        p(d, '1', r(1), r(2), r(3), r(4), r(5), r(6));
        assert r = "110001" report "d1" severity failure;
        -- A null slice written with numbers, in an ascending object
        assert (or up(5 to 4)) = '0' severity failure;
        assert (and up(5 to 4)) = '1' severity failure;
        assert (and up(4 to 5)) = '1' severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;

end architecture;
