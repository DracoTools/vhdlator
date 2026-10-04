-- DESCRIPTION: Verilator: VHDL literal operands of an operator the design
-- declares
--
-- X"05" * X"07" calls a "*" declared over unconstrained BIT_VECTORs, whose
-- body declares "op1 : bit_vector(bv1'range)" and hands it on to a
-- procedure that writes slices of a variable sized from it. The copy made
-- for the call has to know each literal is as long as it is written, eight
-- bits from 0 (9.3.2), or nothing below it has a length either (VESTS
-- ashenden ch_10_bvat-b, ch_08_fg_08_08).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

package t_vhdl_oplitlen_pkg is
    procedure addu (bv1, bv2 : in bit_vector; bv_result : out bit_vector; overflow : out boolean);
    procedure multu (bv1, bv2 : in bit_vector; bv_result : out bit_vector; overflow : out boolean);
    function multu (bv1, bv2 : in bit_vector) return bit_vector;
    function "*" (bv1, bv2 : in bit_vector) return bit_vector;
end package;

package body t_vhdl_oplitlen_pkg is
    procedure addu (bv1, bv2 : in bit_vector; bv_result : out bit_vector; overflow : out boolean) is
        alias op1 : bit_vector(bv1'length - 1 downto 0) is bv1;
        alias op2 : bit_vector(bv2'length - 1 downto 0) is bv2;
        variable result : bit_vector(bv_result'length - 1 downto 0);
        variable carry : bit := '0';
    begin
        for index in result'reverse_range loop
            result(index) := op1(index) xor op2(index) xor carry;
            carry := (op1(index) and op2(index)) or (carry and (op1(index) xor op2(index)));
        end loop;
        bv_result := result;
        overflow := carry = '1';
    end procedure;

    procedure multu (bv1, bv2 : in bit_vector; bv_result : out bit_vector; overflow : out boolean) is
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
        overflow := false;
    end procedure;

    function multu (bv1, bv2 : in bit_vector) return bit_vector is
        variable result : bit_vector(bv1'range);
        variable o : boolean;
    begin
        multu(bv1, bv2, result, o);
        return result;
    end function;

    function "*" (bv1, bv2 : in bit_vector) return bit_vector is
        variable op1 : bit_vector(bv1'range) := bv1;
        variable op2 : bit_vector(bv2'range) := bv2;
    begin
        return multu(op1, op2);
    end function;
end package body;

use work.t_vhdl_oplitlen_pkg.all;

entity t_vhdl_oplitlen is
end entity;

architecture test of t_vhdl_oplitlen is
begin
    process is
        variable byte : bit_vector(0 to 7);
        variable nib : bit_vector(3 downto 0);
    begin
        byte := X"05" * X"07";
        assert byte = X"23" report "byte " & to_string(byte) severity failure;
        nib := "0011" * "0101";
        assert nib = "1111" report "nib " & to_string(nib) severity failure;
        report "*-* All Finished *-*";
        wait;
    end process;
end architecture;
