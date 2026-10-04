-- DESCRIPTION: Verilator: VHDL out parameter of a composite type that the
-- procedure does not assign
--
-- An array or record may be passed by reference (4.2.2.1), and nvc 1.22.1,
-- GHDL 7.0.0-dev and Questa 2025.2 all pass one so: a procedure that
-- returns before assigning its out parameter leaves the caller's variable
-- as it was, through one level of procedure or two (VESTS ashenden
-- ch_10_bvat-b's "16/0").
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_outkeep is
end entity;

architecture test of t_vhdl_outkeep is
    type rec is record
        a : integer;
        b : bit_vector(3 downto 0);
    end record;

    procedure divide (n, d : in bit_vector; q : out bit_vector; bad : out boolean) is
    begin
        if d = (d'range => '0') then
            bad := true;
            return;
        end if;
        bad := false;
        q := n;
    end procedure;

    procedure divide2 (n, d : in bit_vector; q : out bit_vector; bad : out boolean) is
    begin
        divide(n, d, q, bad);
    end procedure;

    procedure maybe (doit : in boolean; r : out rec) is
    begin
        if doit then
            r := (a => 7, b => "0111");
        end if;
    end procedure;
begin
    process is
        variable byte : bit_vector(7 downto 0) := X"10";
        variable bad : boolean;
        variable r : rec := (a => 3, b => "0011");
    begin
        divide(X"22", X"00", byte, bad);
        assert bad and byte = X"10" report "one level" severity failure;
        divide2(X"33", X"00", byte, bad);
        assert bad and byte = X"10" report "two levels" severity failure;
        divide2(X"44", X"01", byte, bad);
        assert not bad and byte = X"44" report "assigned" severity failure;
        maybe(false, r);
        assert r.a = 3 and r.b = "0011" report "record kept" severity failure;
        maybe(true, r);
        assert r.a = 7 and r.b = "0111" report "record assigned" severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
