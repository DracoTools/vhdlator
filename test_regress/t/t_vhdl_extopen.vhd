-- DESCRIPTION: Verilator: VHDL external names of constants that fold, and
-- of signals whose subtype leaves the bounds open (8.7)
--
-- A constant whose value is known where it is declared is still an object
-- an external name may reach; and an external name whose subtype
-- indication leaves the bounds open has those of the object it denotes.
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_extopen_pkg is
    type t_slvv is array (natural range <>) of bit_vector;
end package;

use work.t_vhdl_extopen_pkg.all;

entity t_vhdl_extopen_inner is
end entity;

architecture rtl of t_vhdl_extopen_inner is
    constant WORDS : natural := 16;
    constant BITS : natural := 12;
    signal ram : t_slvv(0 to WORDS - 1)(BITS - 1 downto 0) := (others => (others => '0'));
begin
    process is
    begin
        wait for 1 ns;
        ram(7) <= x"5a3";
        wait;
    end process;
end architecture;

use work.t_vhdl_extopen_pkg.all;

entity t_vhdl_extopen is
end entity;

architecture rtl of t_vhdl_extopen is
begin
    inner : entity work.t_vhdl_extopen_inner;

    blk : block
        alias WORDS is << constant .t_vhdl_extopen.inner.WORDS : natural >>;
        alias BITS is << constant .t_vhdl_extopen.inner.BITS : natural >>;
        alias unconstrained is << signal .t_vhdl_extopen.inner.ram : t_slvv >>;
        alias constrained is
            << signal .t_vhdl_extopen.inner.ram : t_slvv(0 to WORDS - 1)(BITS - 1 downto 0) >>;
    begin
        process is
        begin
            assert WORDS = 16 severity failure;
            assert BITS = 12 severity failure;
            assert unconstrained'length = 16 severity failure;
            assert unconstrained'element'length = 12 severity failure;
            assert constrained(7) = x"000" severity failure;
            wait for 2 ns;
            assert constrained(7) = x"5a3" severity failure;
            assert unconstrained(7) = x"5a3" severity failure;
            report "*-* All Finished *-*";
            std.env.finish;
        end process;
    end block;
end architecture;
