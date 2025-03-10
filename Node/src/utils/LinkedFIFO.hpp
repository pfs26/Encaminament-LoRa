/*
    Implementació Linked List simple, amb format FIFO.
    Permet inserir elements a la llista (push), eliminar (pop), buscar (find)

    En ser templated (osigui `T` es modificarà pel tipus de dades passat en construir-ho), 
    s'utilitza ".hpp" (header + implementació), ja que implementació s'ha de coneixer en compilar-ho (depèn del tipus)

    https://github.com/ivanseidel/LinkedList
*/

#pragma once
#include <cstddef>

template<typename T>
class LinkedFIFO {
private:
    struct Node {
        T data;
        Node* next;
        Node* prev;
    };

    Node* head;
    Node* tail;
    size_t size;

public:
    // Constructor
    LinkedFIFO() : head(nullptr), tail(nullptr), size(0) {}
    // Destructor (per evitar que quedi memòria ocupada)
    ~LinkedFIFO() {
        clear();
    }
    // Afegir un element a la cua
    void push(const T& value) {
        // Creem un nou node de la cua, amb el valor donat
        Node* node = new Node{value, nullptr, tail};
        if (!tail) { // si no hi havia cap element, el nou inici (head) i final (tail) és el nou node
            head = tail = node;
        } else { 
            tail->next = node; // si no, el següent element de l'últim de la cua és el nou element
            tail = node; // la nova cua és el nou node
        }
        size++;
    }
    // Eliminar un element de la cua (FIFO)
    bool pop(T& value) { 
        if (!head) return false; // si no hi ha cap element
        
        Node* temp = head; // obtenim pointer primer element
        value = head->data; // obtenim dades del primer element
        head = head->next; // el nou head és el següent element de la cua
        
        if (head) { // si hi ha un element següent
            head->prev = nullptr; // el nou head no té element anterior (s'acaba d'eliminar)
        } else {
            tail = nullptr; // Si no hi ha head, cua buida
        }
        
        delete temp; // alliberar memòria
        size--; // reduir mida
        return true;
    }

    size_t count() {
        return size;
    }

    // Eliminar tots els elements de la cua
    void clear() {
        T temp; 
        while (pop(temp));  // pops fins a buidar
    }

    bool isEmpty() const {
        return size == 0;
    }

    size_t getSize() const {
        return size;
    }
};
