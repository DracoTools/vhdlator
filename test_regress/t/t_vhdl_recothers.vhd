-- DESCRIPTION: Verilator: VHDL record aggregate whose "others" covers elements of different lengths
--
-- "(others => (others => '0'))" over a record of a five-bit and a six-bit
-- vector is five zeros and six zeros: "others" stands for each element it
-- covers in that element's own subtype (9.3.3.1). It was built once, at
-- one length, for all of them (GHDL gna issue1323).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_recothers is
    generic (genint : integer := 6);
end entity;

architecture test of t_vhdl_recothers is
    subtype cbv is bit_vector(4 downto 0);
    subtype gbv is bit_vector(genint - 1 downto 0);
    type rcd is record
        c : cbv;
        g : gbv;
    end record;
    type array_of_rcd is array (integer range <>) of rcd;
    subtype ca_rcd is array_of_rcd(3 downto 0);

    type mixed is record
        a : bit_vector(1 to 3);
        n : integer;
        b : bit_vector(0 to 6);
    end record;

    constant k : ca_rcd := (0 => (c => "00111", g => (others => '1')),
                            1 => (c => "11000", g => (others => '0')),
                            others => (others => (others => '0')));
    constant r1 : rcd := (others => (others => '1'));
    constant m : mixed := (n => 4, others => (others => '1'));
begin
    process is
        variable v : rcd;
    begin
        assert k(0).c = "00111" and k(1).c = "11000" and k(3).c = "00000" severity failure;
        assert k(0).g = "111111" and k(1).g = "000000" and k(2).g = "000000" severity failure;
        assert r1.c = "11111" and r1.g = "111111" severity failure;
        assert m.a = "111" and m.n = 4 and m.b = "1111111" severity failure;
        v := (others => (others => '1'));
        assert v.c'length = 5 and v.g = "111111" severity failure;
        report "*-* All Finished *-*";
        std.env.finish;
    end process;
end architecture;
