-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A shift-and-add multiplier the way Ashenden's bv_arithmetic writes one
-- (VESTS ch_10_chkmult): each partial sum is added into a slice of the
-- accumulator whose ends move with the loop, "accum(count + len - 1 downto
-- count)", by a procedure that renumbers its operands with "alias op1 :
-- bit_vector(bv1'length - 1 downto 0) is bv1". The slice was handed over as
-- long as the whole accumulator, and the alias counted on the slice's own
-- bounds rather than from its left end.
entity t_vhdl_aliasmove is
end entity;

architecture test of t_vhdl_aliasmove is
    procedure addu(bv1, bv2 : in bit_vector; bv_result : out bit_vector; overflow : out boolean) is
        alias op1 : bit_vector(bv1'length - 1 downto 0) is bv1;
        alias op2 : bit_vector(bv2'length - 1 downto 0) is bv2;
        variable result : bit_vector(bv_result'length - 1 downto 0);
        variable carry : bit := '0';
    begin
        assert bv1'length = bv2'length and bv1'length = bv_result'length
            report "operands of different lengths" severity failure;
        for index in result'reverse_range loop
            result(index) := op1(index) xor op2(index) xor carry;
            carry := (op1(index) and op2(index)) or (carry and (op1(index) xor op2(index)));
        end loop;
        bv_result := result;
        overflow := carry = '1';
    end procedure;

    procedure multu(bv1, bv2 : in bit_vector; bv_result : out bit_vector; overflow : out boolean) is
        alias op1 : bit_vector(bv1'length - 1 downto 0) is bv1;
        alias op2 : bit_vector(bv2'length - 1 downto 0) is bv2;
        constant len : natural := bv1'length;
        variable accum : bit_vector(2 * len - 1 downto 0) := (others => '0');
        variable o : boolean;
    begin
        for count in 0 to len - 1 loop
            if op2(count) = '1' then
                addu(accum(count + len - 1 downto count), op1, accum(count + len - 1 downto count), o);
                accum(count + len) := bit'val(boolean'pos(o));
            end if;
        end loop;
        bv_result := accum(len - 1 downto 0);
        overflow := accum(2 * len - 1 downto len) /= (2 * len - 1 downto len => '0');
    end procedure;

    function to_bv(n, len : natural) return bit_vector is
        variable r : bit_vector(len - 1 downto 0);
        variable v : natural := n;
    begin
        for i in 0 to len - 1 loop
            r(i) := bit'val(v mod 2);
            v := v / 2;
        end loop;
        return r;
    end function;

    function to_nat(bv : bit_vector) return natural is
        variable r : natural := 0;
    begin
        for i in bv'range loop
            r := r * 2 + bit'pos(bv(i));
        end loop;
        return r;
    end function;
begin
    process
        variable a, b, p : bit_vector(3 downto 0);
        variable ov : boolean;
    begin
        for i in 0 to 15 loop
            for j in 0 to 15 loop
                a := to_bv(i, 4);
                b := to_bv(j, 4);
                multu(a, b, p, ov);
                assert ov = (i * j > 15)
                    report integer'image(i) & "*" & integer'image(j) & ": overflow"
                    severity failure;
                assert i * j > 15 or to_nat(p) = i * j
                    report integer'image(i) & "*" & integer'image(j) & " = "
                           & integer'image(to_nat(p))
                    severity failure;
            end loop;
        end loop;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
